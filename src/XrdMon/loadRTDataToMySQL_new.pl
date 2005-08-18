#!/usr/local/bin/perl -w

use DBI;
use Fcntl;

###############################################################################
#                                                                             #
#                            loadRTDataToMySQL.pl                             #
#                                                                             #
#  (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  #
#                             All Rights Reserved                             #
#        Produced by Jacek Becla for Stanford University under contract       #
#               DE-AC02-76SF00515 with the Department of Energy               #
###############################################################################

# $Id$


# take care of arguments
if ( @ARGV!=1 ) {
    print "Expected argument <configFile>\n";
    exit;
}
$confFile = $ARGV[0];
unless ( open INFILE, "< $confFile" ) {
    print "Can't open file $confFile\n";
    exit;
}
while ( $_ = <INFILE> ) {
    chomp();
    my ($token, $v1, $v2) = split(/ /, $_);
    if ( $token =~ "dbName:" ) {
	$dbName = $v1;
    } elsif ( $token =~ "MySQLUser:" ) {
	$mySQLUser = $v1;
    } elsif ( $token =~ "inputDir:" ) {
	$inputDir = $v1;
    } elsif ( $token =~ "jrnlDir:" ) {
	$jrnlDir = $v1;
    } elsif ( $token =~ "backupDir:" ) {
	$backupDir = $v1;
    } elsif ( $token =~ "backupInt:" ) {
        $backupInts{$v1} = $v2;
    } else {
	print "Invalid entry: \"$_\"\n";
        close INFILE;
	exit;
    }
}
close INFILE;

# do the necessary one-time initialization

$jrnlDir = $jrnlDir;
if ( ! -d $jrnlDir ) { 
    mkdir $jrnlDir;
}

if ( ! -d $backupDir ) {
    mkdir $backupDir;
}

unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
    print "Error while connecting to database. $DBI::errstr\n";
    return;
}


@siteNames = &runQueryRetArray("SELECT name FROM sites");
foreach $siteName (@siteNames) {
    my $inFN = "$inputDir/$siteName.ascii";
    if ( -l $inFN ) {
        $siteInputFiles{$siteName} = readlink $inFN;
        $link = readlink $inFN;
        $ch1 = substr $link, 0, 1;
        if ( $ch1 =~ "." ) {
             $siteInputFiles{$siteName} = "$inputDir/$link";
        } else {
             $siteInputFiles{$siteName} = $link;
        }
        if ( ! -e $siteInputFiles{$siteName} ) {
             print "Input file$siteInputFiles{$siteName} does not exist \n";
             exit;
        }
    } elsif ( -e  $inFN ) {
        $siteInputFiles{$siteName} = $inFN;
    } else {
        print "Need to supply input file or link to it for site $siteName\n";
        exit;
    }
    ($siteIds{$siteName}, $backupInts{$siteName}, $backupTime) = 
                         &runQueryWithRet("SELECT id, backupInt, backupTime
                                             FROM sites 
                                            WHERE name = \"$siteName\"");
   
    if ( ! -d "$jrnlDir/$siteName" ) { 
        mkdir "$jrnlDir/$siteName";
    }
    if ( ! -d "$backupDir/$siteName" ) { 
        mkdir "$backupDir/$siteName";
    }

    ($bkupdate, $bkuptime) = split / /, "$backupTime";
    $backupFile = "$backupDir/$siteName/${siteName}-${bkupdate}-${bkuptime}-GMT.backup";
    $backupFiles{$siteName} = $backupFile

}

$dbh->disconnect();

my $stopFName = "$confFile.stop";
@primes = (101, 127, 157, 181, 199, 223, 239, 251, 271, 307);
%timeZones = ( "SLAC", "PST8PDT", "RAL", "WET");
&recover();

# and run the main never-ending loop
while ( 1 ) {
    &doLoading();

    my $sec2Sleep = 60 - (localtime)[0];
    print "sleeping $sec2Sleep sec... \n";
    sleep $sec2Sleep; 

    if ( -e $stopFName ) {
	unlink $stopFName;
	exit;
    }
}

###############################################################################
###############################################################################
###############################################################################
sub recover {
    my $ts = &timestamp();

    # connect to the database
    print "\n$ts Connecting to database...\n";
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	return;
    }

    # recover data for each site
    my $nr = 0;
    foreach $siteName (@siteNames) {
        my $inFN = "$jrnlDir/$siteName/$siteName.ascii";
        # recover u/o/d/c files BUT NOT if inout file exists in jrnl directory
        # since they will be remade when doLoading is first called
        if ( ! -e $inFN ) {
             print "Checking for pending u/o/d/c files for $siteName \n";
             if ( -e  "$jrnlDir/$siteName/ufile.ascii" ) {
                &loadOpenSession($siteName);
             }
             if ( -e  "$jrnlDir/$siteName/ofile.ascii" ) {
                &loadOpenFile($siteName);
             }
             if ( -e  "$jrnlDir/$siteName/dfile.ascii" ) {
                &loadCloseSession($siteName);
             }
             if ( -e  "$jrnlDir/$siteName/cfile.ascii" ) {
                &loadCloseFile($siteName);
             }
         }
     }
}           
             

sub doLoading {
    my $ts = &timestamp();

    # connect to the database
    print "\n$ts Connecting to database...\n";
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	return;
    }

    # load data for each site
    my $gmts = &gmtimestamp();
    my $nr = 0;
    foreach $siteName (@siteNames) {
	$nr += loadOneSite($siteName, $gmts);
    }

    # disconnect from db
    $dbh->disconnect();

    $ts = &timestamp();
    print "$ts All done, processed $nr entries.\n";

}

sub loadOneSite() {
    use vars qw($nextBackup);
    my ($siteName, $loadTime) = @_;

    print "Loading for --> $siteName <--\n";

    my $inFN   = $siteInputFiles{$siteName};
    my $siteId = $siteIds{$siteName};
    my $backupInt = $backupInts{$siteName};

    # use the input file only if there is none in jrnl directory after a crash
    if ( ! -e "$jrnlDir/$siteName/$siteName.ascii" ) {

        if ( ! -e $inFN || -z $inFN  ) {
            print "File $inFN does not exist or is empty \n";
       	    return 0;
        }
        # lock the file
        unless ( $lockF = &lockTheFile($inFN) ) {
            return 0;
        }
        # make a backup of the input file and move it to jrnl directory
        $nextBackup = &runQueryWithRet("SELECT DATE_ADD(backupTime, INTERVAL $backupInt)
                                        FROM sites
                                        WHERE name = '$siteName'");
        if ( $loadTime ge $nextBackup ) {
             &runQuery("UPDATE sites
                        SET backupTime = \"$loadTime\"
                        WHERE name = '$siteName'");
                           
             ($bkupdate, $bkuptime) = split / /, "$loadTime";
             $backupFile = "$backupDir/$siteName/${siteName}-${bkupdate}-${bkuptime}-GMT.backup";
             $backupFiles{$siteName} = $backupFile
        } else {
             $backupFile = $backupFiles{$siteName}
        }
        `touch $backupFile; cat $inFN >> $backupFile; mv $inFN $jrnlDir/$siteName/$siteName.ascii`;

        # unlock the lock file
        unlockTheFile($lockF);
    }    

    $inFN  = "$jrnlDir/$siteName/$siteName.ascii";
    # open the input file for reading
    unless ( open INFILE, "< $inFN" ) {
	print "Can't open file $inFN for reading\n";
	return 0;
    }
    # read the file, sort the data, close the file
    open OFILE, ">$jrnlDir/$siteName/ofile.ascii" or die "can't open ofile.ascii for write: $!";
    open UFILE, ">$jrnlDir/$siteName/ufile.ascii" or die "can't open ufile.ascii for write: $!";
    open DFILE, ">$jrnlDir/$siteName/dfile.ascii" or die "can't open dfile.ascii for write: $!";
    open CFILE, ">$jrnlDir/$siteName/cfile.ascii" or die "can't open cfile.ascii for write: $!";
    print "Sorting...\n";
    my $nr = 0;
    while ( <INFILE> ) {
        if ( $_ =~ m/^u/ ) { print UFILE $_; }
        if ( $_ =~ m/^d/ ) { print DFILE $_; }
        if ( $_ =~ m/^o/ ) { print OFILE $_; }
        if ( $_ =~ m/^c/ ) { print CFILE $_; }
        $nr++;
    }

    close INFILE;
    `rm $inFN`;
    
    close OFILE;
    close UFILE;
    close DFILE;
    close CFILE;

    print "Loading...\n";
    &loadOpenSession($siteName);
    &loadOpenFile($siteName);
    &loadCloseSession($siteName);
    &loadCloseFile($siteName);
    
    return $nr;
}

# opens the <fName>.lock file for writing & locks it (write lock)
sub lockTheFile() {
    my ($fName) = @_;

    my $lockFName = "$fName.lock";
    print "Locking $lockFName...\n";
    unless ( open($lockF, "> $lockFName") ) {
	print "Can't open file $lockFName 4 writing\n";
	return;
    }
    my $lk_parms = pack('sslllll', F_WRLCK, 0, 0, 0, 0, 0, 0);
    fcntl($lockF, F_SETLKW, $lk_parms) or die "can't fcntl F_SETLKW: $!";
    return $lockF;
}

sub unlockTheFile() {
    my ($fh) = @_;
    my $lk_parms = pack('sslllll', F_UNLCK, 0, 0, 0, 0, 0, 0);
    fcntl($fh, F_SETLKW, $lk_parms);
}

sub loadOpenSession() {
    my ($siteName) = @_;

    print "Loading open sessions...\n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.u";
    my $inFile = "$jrnlDir/$siteName/ufile.ascii";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;
        my ($u, $sessionId, $user, $pid, $clientHost, $srvHost) = split('\t');
        my $userId       = findOrInsertUserId($user, $siteName);
        my $clientHostId = findOrInsertHostId($clientHost, $siteName);
        my $serverHostId = findOrInsertHostId($srvHost, $siteName);

        #print "uid=$userId, chid=$clientHostId, shd=$serverHostId\n";
        print MYSQLIN "$sessionId \t  $userId \t $pid \t ";
        print MYSQLIN "$clientHostId \t $serverHostId \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_openedSessions");
    print "... $rows rows loaded \n";
    `rm $mysqlIn $inFile`;
}


sub loadCloseSession() {
    my ($siteName) = @_;

    print "Loading closed sessions... \n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.d";
    my $inFile = "$jrnlDir/$siteName/dfile.ascii";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($d, $sessionId, $sec, $timestamp) = split('\t');
        #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";


        # find if there is corresponding open session, if not don't bother
        my ($userId, $pId, $clientHId, $serverHId) = 
	   &runQueryWithRet("SELECT userId, pId, clientHId, serverHId 
                             FROM ${siteName}_openedSessions
                             WHERE id = $sessionId");
        next if ( ! $pId  );

        # remove it from the open session table
        &runQuery("DELETE FROM ${siteName}_openedSessions 
                   WHERE id = $sessionId");

        # and insert into the closed
        print MYSQLIN "$sessionId \t $userId \t $pId \t ";
        print MYSQLIN "$clientHId \t $serverHId \t $sec \t $timestamp \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

##################################################################### TEMPORARY CODE
    &runQuery("CREATE TEMPORARY TABLE closedSessions LIKE SLAC_closedSessions");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" INTO TABLE closedSessions");
    &runQuery("UPDATE closedSessions SET disconnectT = CONVERT_TZ(disconnectT, \"$timeZones{$siteName}\", \"GMT\") ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions_LastHour 
                    SELECT * FROM closedSessions                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions_LastDay
                    SELECT * FROM closedSessions                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions_LastWeek
                    SELECT * FROM closedSessions                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions_LastMonth
                    SELECT * FROM closedSessions                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions_LastYear
                    SELECT * FROM closedSessions                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions
                    SELECT * FROM closedSessions                      ");
    &runQuery("DROP TABLE IF EXISTS closedSessions                    ");
##################################################################### TEMPORARY CODE
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE
#              INTO TABLE ${siteName}_closedSessions          ");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedSessions_LastHour ");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedSessions_LastDay  ");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedSessions_LastWeek ");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedSessions_LastMonth");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedSessions_LastYear ");

    print "$rows rows loaded \n";
    `rm $mysqlIn $inFile`;
}


sub loadOpenFile() {
    my ($siteName) = @_;

    print "Loading opened files...\n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.o";
    my $inFile = "$jrnlDir/$siteName/ofile.ascii";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($o,$fileId,$user,$pid,$clientHost,$path,$openTime,$srvHost) = 
	    split('\t');

        my $sessionId = 
	    findSessionId($user,$pid,$clientHost,$srvHost,$siteName);
        if ( ! $sessionId ) {
	    next; # error: no corresponding session id
        }

        my $pathId = findOrInsertPathId($path);
        #print "$sessionId $pathId \n";
        if ( ! $pathId ) {
             print "path id not found for $path \n";
            next;
        }
        
        print MYSQLIN "$fileId \t $sessionId \t $pathId \t $openTime \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

##################################################################### TEMPORARY CODE
    &runQuery("CREATE TEMPORARY TABLE openedFiles LIKE SLAC_openedFiles");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" INTO TABLE openedFiles");
    &runQuery("UPDATE openedFiles SET openT = CONVERT_TZ(openT, \"$timeZones{$siteName}\", \"GMT\") ");
    &runQuery("INSERT IGNORE INTO ${siteName}_openedFiles
                    SELECT * FROM openedFiles                      ");
    &runQuery("DROP TABLE IF EXISTS openedFiles                    ");
##################################################################### TEMPORARY CODE

#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_openedFiles");
    print "$rows rows loaded \n";
    `rm $mysqlIn $inFile`;

}

sub loadCloseFile() {
    my ($siteName) = @_;

    print "Loading closed files... \n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.c";
    my $inFile = "$jrnlDir/$siteName/cfile.ascii";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t');
        #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

        # find if there is corresponding open file, if not don't bother
        my ($sessionId, $pathId, $openT) = 
	    &runQueryWithRet("SELECT sessionId, pathId, openT 
                              FROM ${siteName}_openedFiles
                              WHERE id = $fileId");
        next if ( ! $sessionId );

        # remove it from the open files table
        &runQuery("DELETE FROM ${siteName}_openedFiles WHERE id = $fileId");

        # and insert into the closed
        print MYSQLIN "$fileId \t $sessionId \t $pathId \t  ";
        print MYSQLIN "$openT \t  $closeT \t $bytesR \t $bytesW \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

##################################################################### TEMPORARY CODE
    &runQuery("CREATE TEMPORARY TABLE closedFiles LIKE SLAC_closedFiles");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" INTO TABLE closedFiles");
    &runQuery("UPDATE closedFiles SET openT  = CONVERT_TZ( openT, \"$timeZones{$siteName}\", \"GMT\") ");
    &runQuery("UPDATE closedFiles SET closeT = CONVERT_TZ(closeT, \"$timeZones{$siteName}\", \"GMT\") ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles
                    SELECT * FROM closedFiles                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastHour
                    SELECT * FROM closedFiles                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastDay
                    SELECT * FROM closedFiles                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastWeek
                    SELECT * FROM closedFiles                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastMonth
                    SELECT * FROM closedFiles                      ");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastYear
                    SELECT * FROM closedFiles                      ");

    &runQuery("DROP TABLE IF EXISTS closedFiles                    ");
##################################################################### TEMPORARY CODE

#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE
#              INTO TABLE ${siteName}_closedFiles         ");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedFiles_LastHour");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedFiles_LastDay");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedFiles_LastWeek");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedFiles_LastMonth");
#   &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
#              INTO TABLE ${siteName}_closedFiles_LastYear");
    print "$rows rows loaded \n";
    `rm $mysqlIn $inFile`;
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost, $siteName) = @_;

    my $userId       = findOrInsertUserId($user, $siteName);
    my $clientHostId = findOrInsertHostId($clientHost, $siteName);
    my $serverHostId = findOrInsertHostId($srvHost, $siteName);

    return &runQueryWithRet("SELECT id FROM ${siteName}_openedSessions 
                                       WHERE     userId=$userId 
                                             AND pId=$pid 
                                             AND clientHId=$clientHostId 
                                             AND serverHId=$serverHostId");
}


sub findOrInsertUserId() {
    my ($userName, $siteName) = @_;

    my $userId = $userIds{$userName};
    if ( $userId ) {
        return $userId;
    }
    $userId = &runQueryWithRet("SELECT id 
                                FROM ${siteName}_users
                                WHERE name = \"$userName\"");
    if ( $userId ) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO ${siteName}_users (name)
                   VALUES (\"$userName\")");

	$userId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
    }
    $userIds{$userName} = $userId;
    return $userId;
}

sub findOrInsertHostId() {
    my ($hostName, $siteName) = @_;

    my $hostId = $hostIds{$hostName};
    if ( $hostId ) {
        return $hostId;
    }
    $hostId = &runQueryWithRet("SELECT id
                                FROM ${siteName}_hosts
                                WHERE hostName = \"$hostName\"");
    if ( $hostId ) {
	#print "Will reuse hostId $clientHostId for $hostName\n";
    } else {
	#print "$hostName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO ${siteName}_hosts (hostName)
                   VALUES (\"$hostName\")");

	$hostId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
    }
    $hostIds{$hostName} = $hostId;
    return $hostId;
}

sub findOrInsertPathId() {
    my ($path) = @_;

    use vars qw($pathId $typeId $skimId);

    $pathId = $pathIds{$path};
    if ( $pathId ) {
        #print "from cache: $pathId for $path\n";
        return $pathId;
    }
    my $hashValue = &returnHash("$path");
    ($pathId, $typeId, $skimId) =
        &runQueryWithRet("SELECT id, typeId, skimId FROM paths 
                          WHERE hash = $hashValue AND name = \"$path\"");

    # split path and find file type and skim name
    my @sections = split(/\//, $path);
    my $typeName = $sections[2];
    my $skimName = $sections[5];

    if ( $pathId ) {
        #print "Will reuse pathId for $path\n";
    } else {
        #print "$path not in mysql yet, inserting...\n";

        $typeId = 0;
        $skimId = 0;

        # find if the type has already id, reuse if it does
        $typeId = $fileTypes{$typeName};
        if ( ! $typeId ) {
            $typeId = &runQueryWithRet("SELECT id
                                        FROM fileTypes
                                        WHERE name = \"$typeName\"");
        }
        if ( ! $typeId ) {
            &runQuery("INSERT INTO fileTypes(name) VALUES(\"$typeName\")");
            $typeId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
        }
        # if it is skim, deal with the skim type, if not, 0 would do
        if ( $typeName =~ /skims/ ) {
            # find if the skim name has already id, reuse if it does
            $skimId = $skimNames{$skimName};
            if ( ! $skimId ) {
                $skimId = &runQueryWithRet("SELECT id
                                            FROM skimNames
                                            WHERE name = \"$skimName\"");
            }
            if ( ! $skimId ) {
                &runQuery("INSERT INTO skimNames(name) VALUES(\"$skimName\")");
                $skimId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
            }
        }
        &runQuery("INSERT INTO paths (name,typeId,skimId,size,hash)
                   VALUES (\"$path\", $typeId, $skimId, 0, $hashValue )");
        $pathId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
    }
    $pathIds{$path} = $pathId;
    if ( ! $fileTypes{$typeName} ) { 
        $fileTypes{$typeName} = $typeId;
    }
    if ( $typeName =~ /skims/  and ! $skimNames{$skimName} ) { 
        $skimNames{$skimName} = $skimId;
    }
    return $pathId;
}

sub runQueryWithRet() {
    my $sql = shift @_;
#    print "$sql;\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
    return $sth->fetchrow_array;
}

sub runQueryRetArray() {
    use vars qw(@theArray);
    my $sql = shift @_;
    @theArray = ();   
#    print "$sql;\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";

    while ( @x = $sth->fetchrow_array ) {
	push @theArray, @x;
    };
    return @theArray;
}

sub runQuery() {
    my ($sql) = @_;
#    print "$sql;\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
}

sub timestamp() {
    my @localt = localtime(time());
    my $sec    = $localt[0];
    my $min    = $localt[1];
    my $hour   = $localt[2];
    my $day    = $localt[3];
    my $month  = $localt[4] + 1;
    my $year   = $localt[5] + 1900;

    return sprintf("%04d-%02d-%02d %02d:%02d:%02d",
                   $year, $month, $day, $hour, $min, $sec);
}


sub gmtimestamp() {
    my @gmt   = gmtime(time());
    my $sec   = $gmt[0];
    my $min   = $gmt[1];
    my $hour  = $gmt[2];
    my $day   = $gmt[3];
    my $month = $gmt[4] + 1;
    my $year  = $gmt[5] + 1900;

    return sprintf("%04d-%02d-%02d %02d:%02d:%02d", 
                   $year, $month, $day, $hour, $min, $sec);
}

sub returnHash() {
    ($_) = @_;
    my $i = 1;
    tr/0-9a-zA-Z/0-90-90-90-90-90-90-1/;
    tr/0-9//cd;
    my $hashValue = 0;
    foreach $char ( split / */ ) {
	$i++;
      # $primes initialized in doInit()
	$hashValue += $i * $primes[$char];
    }
    return $hashValue;
}
