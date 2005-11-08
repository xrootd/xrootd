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
$mysqlSocket = '/tmp/mysql.sock';
$maxIdleTime = 900; # 15 min

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
    } elsif ( $token =~ "MySQLSocket:" ) {
        $mysqlSocket = $v1;
    } elsif ( $token =~ "maxIdleTime:" ) {
	$maxIdleTime = $v1;
    } else {
	print "Invalid entry: \"$_\"\n";
        close INFILE;
	exit;
    }
}
close INFILE;

# do the necessary one-time initialization

if ( ! -d $jrnlDir ) { 
    mkdir $jrnlDir;
}

if ( ! -d $backupDir ) {
    mkdir $backupDir;
}

unless ( $dbh = DBI->connect("dbi:mysql:$dbName;mysql_socket=$mysqlSocket",$mySQLUser) ) {
    print "Error while connecting to database. $DBI::errstr\n";
    return;
}


@siteNames = &runQueryRetArray("SELECT name FROM sites");
foreach $siteName (@siteNames) {
    my $inFN = "$inputDir/$siteName.ascii";
    if ( -l $inFN ) {
        $link = readlink $inFN;
        if ( $link =~ "^/" ) {
             $siteInputFiles{$siteName} = $link;
        } else {
             $siteInputFiles{$siteName} = "$inputDir/$link";
        }
        if ( ! -e $siteInputFiles{$siteName} ) {
             print "Input file $siteInputFiles{$siteName} does not exist \n";
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
    my $minSec1 = (localtime)[1]*60+(localtime)[0];
    my $sec2Sleep = 60 - (localtime)[0];
    if ( $sec2Sleep < 60 ) {
        print "sleeping $sec2Sleep sec... \n";
        sleep $sec2Sleep;
    } 

    &doLoading();

    if ( -e $stopFName ) {
	unlink $stopFName;
	exit;
    }

    my $minSec2 = (localtime)[1]*60+(localtime)[0];
    if ( $minSec2 == $minSec1 ) {
        sleep(2);
    }
}

###############################################################################
###############################################################################
###############################################################################
sub recover {
    my $ts = &timestamp();
    my $sec2Sleep = 60 - (localtime)[0];
    if ( $sec2Sleep < 60 ) {
        print "sleeping $sec2Sleep sec... \n";
        sleep $sec2Sleep;
    } 

    $loadTime = &gmtimestamp();
    
    # connect to the database
    print "\n$ts Connecting to database...\n";
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName;mysql_socket=$mysqlSocket",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	return;
    }

    # recover data for each site
    my $nr = 0;
    foreach $siteName (@siteNames) {
        my $inFN = "$jrnlDir/$siteName/$siteName.ascii";
        # recover u/o/d/c/r files BUT NOT if inout file exists in jrnl directory
        # since they will be remade when doLoading is first called
        if ( ! -e $inFN ) {
             my $version = &runQueryWithRet("SELECT version
                                               FROM sites
                                              WHERE name = '$siteName' ");
             foreach $v ( 1 .. $version ) {
                 print "Checking for pending u/o/d/c/r files for $siteName version $v \n";
                 if ( -e  "$jrnlDir/$siteName/ufile-V${v}.ascii" ) {
                    &loadOpenSession($siteName, $loadTime, $v);
                 }
                 if ( -e  "$jrnlDir/$siteName/ofile-V${v}.ascii" ) {
                    &loadOpenFile($siteName, $v);
                 }
                 if ( -e  "$jrnlDir/$siteName/cfile-V${v}.ascii" ) {
                    &loadCloseFile($siteName, $v);
                 }
                 if ( -e  "$jrnlDir/$siteName/dfile-V${v}.ascii" ) {
                    &loadCloseSession($siteName, $v);
                 }
                 if ( -e  "$jrnlDir/$siteName/rfile-V${v}.ascii" ) {
                    &loadXrdRestarts($siteName, $v);
                 }
             }
         }
         &runQuery("UPDATE sites 
                       SET dbUpdate = '$loadTime'
                     WHERE name = '$siteName' ");
     }
}           
             

sub doLoading {
    my $ts = &timestamp();

    # connect to the database
    print "\n$ts Connecting to database...\n";
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName;mysql_socket=$mysqlSocket",$mySQLUser) ) {
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
    $version = &runQueryWithRet("SELECT version
                                   FROM sites
                                  WHERE name = '$siteName' ");
    # read the file, sort the data, close the file
    &openInputFiles($siteName, $version );
    print "Sorting...\n";
    my $nr = 0;
    my @versions = ($version);
    while ( <INFILE> ) {
        if    ( $_ =~ m/^o/ ) { print OFILE $_; }
        elsif ( $_ =~ m/^u/ ) { print UFILE $_; }
        elsif ( $_ =~ m/^d/ ) { print DFILE $_; }
        elsif ( $_ =~ m/^c/ ) { print CFILE $_; }
        elsif ( $_ =~ m/^r/ ) { print RFILE $_; }
        elsif ( $_ =~ m/^v/ ) {
            my ($v, $newVersion) =  split('\t');
            next if ( $newVersion == $version ); 
            $version = $newVersion;
            @versions = (@versions, $version);
            &runQuery("UPDATE sites
                          SET version = $version
                        WHERE name = '$siteName' ");
            &closeInputFiles();
            &openInputFiles($siteName, $version );
        }
        $nr++;
    }

    close INFILE;
    `rm $inFN`;

    &closeInputFiles();

    print "Loading...\n";
    foreach $version ( @versions ) {
        &loadOpenSession($siteName, $loadTime, $version);
        &loadOpenFile($siteName, $version);
        &loadCloseFile($siteName, $version);
        &loadCloseSession($siteName, $version);
        &loadXrdRestarts($siteName, $version);
    }
    # record loadTime in sites table
    &runQuery("UPDATE sites 
                  SET dbUpdate = '$loadTime'
                WHERE name = '$siteName' ");
    return $nr;
}

sub openInputFiles() {
    my ($siteName, $version) = @_;
    open OFILE, ">$jrnlDir/$siteName/ofile-V${version}.ascii" or die "can't open ofile.ascii for write: $!";
    open UFILE, ">$jrnlDir/$siteName/ufile-V${version}.ascii" or die "can't open ufile.ascii for write: $!";
    open DFILE, ">$jrnlDir/$siteName/dfile-V${version}.ascii" or die "can't open dfile.ascii for write: $!";
    open CFILE, ">$jrnlDir/$siteName/cfile-V${version}.ascii" or die "can't open cfile.ascii for write: $!";
    open RFILE, ">$jrnlDir/$siteName/rfile-V${version}.ascii" or die "can't open rfile.ascii for write: $!";
}

sub closeInputFiles() {
    close OFILE;
    close UFILE;
    close DFILE;
    close CFILE;
    close RFILE;
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
    my ($siteName, $loadTime, $version) = @_;
    my $inFile = "$jrnlDir/$siteName/ufile-V${version}.ascii";
    if ( -z $inFile ) {return;}
    print "Loading open sessions version $version...\n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.u";
    
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;
        my ($u, $sessionId, $user, $pid, $clientHost, $srvHost) = split('\t');
        my $userId       = findOrInsertUserId($user, $siteName);
        my $clientHostId = findOrInsertHostId($clientHost, $siteName);
        my $serverHostId = findOrInsertHostId($srvHost, $siteName);
        my $jobId        = &runQueryWithRet("SELECT jobId 
                                               FROM ${siteName}_jobs
                                              WHERE userId    = $userId        AND
                                                    pId       = $pid           AND
                                                    clientHId = $clientHostId  AND
                                                    ( noOpenSessions > 0         OR
                                                      '$loadTime' <= DATE_ADD(endT, INTERVAL $maxIdleTime SECOND) )
                                           ORDER BY jobId DESC
                                              LIMIT 1     ");
        if ( $jobId ) {
            &runQuery("UPDATE ${siteName}_jobs   SET noOpenSessions = noOpenSessions + 1,
                                                     beginT         = LEAST( '$loadTime', beginT)
                                               WHERE      jobId = $jobId");
        } else {
            &runQuery("INSERT INTO ${siteName}_jobs ( userId,  pId,  clientHId, noOpenSessions, beginT,     endT    ) 
                            VALUES                  ($userId, $pid, $clientHostId,      1    , '$loadTime', '$loadTime')");
            $jobId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
        }
        #print "uid=$userId, chid=$clientHostId, shd=$serverHostId, jobId\n";
        print MYSQLIN "$sessionId \t  $jobId \t $userId \t $pid \t ";
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
    my ($siteName, $version) = @_;

    my $inFile = "$jrnlDir/$siteName/dfile-V${version}.ascii";
    if ( -z $inFile ) {return;}

    print "Loading closed sessions version $version... \n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.d";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($d, $sessionId, $sec, $timestamp) = split('\t');
        #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";


        # find if there is corresponding open session, if not don't bother
        my ($jobId, $userId, $pId, $clientHId, $serverHId) = 
	   &runQueryWithRet("SELECT jobId, userId, pId, clientHId, serverHId 
                             FROM ${siteName}_openedSessions
                             WHERE id = $sessionId");
        next if ( ! $pId  );
        # update jobs table
        if ( $version == 1 ) {
            $timestamp = &runQueryWithRet("SELECT CONVERT_TZ('$timestamp', '$timeZones{$siteName}', 'GMT') ");
        }
        &runQuery("UPDATE ${siteName}_jobs  SET noOpenSessions = noOpenSessions - 1, 
                                                beginT = LEAST(beginT, DATE_SUB('$timestamp', INTERVAL $sec SECOND)),
                                                endT   = GREATEST(endT, '$timestamp')
                                          WHERE jobId  = $jobId");
        
        # remove it from the open session table
        &runQuery("DELETE FROM ${siteName}_openedSessions 
                   WHERE id = $sessionId");

        # and insert into the closed
        print MYSQLIN "$sessionId \t $jobId \t $userId \t $pId \t ";
        print MYSQLIN "$clientHId \t $serverHId \t $sec \t $timestamp \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE
               INTO TABLE ${siteName}_closedSessions          ");
    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastHour ");
    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastDay  ");
    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastWeek ");
    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastMonth");
    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastYear ");
    
    print "$rows rows loaded \n";
    `rm $mysqlIn $inFile`;
}


sub loadOpenFile() {
    my ($siteName, $version) = @_;
    use vars qw($o $fileId $user $pid $clientHost $path $openT $size $srvHost);

    my $inFile = "$jrnlDir/$siteName/ofile-V${version}.ascii";
    if ( -z $inFile ) {return;}

    print "Loading opened files version $version...\n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.o";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;
 
        if ( $version == 1 ) {
            ($o,$fileId,$user,$pid,$clientHost,$path,$openT,$srvHost) = split('\t');
            $size = 0;
        } else {
            ($o,$fileId,$user,$pid,$clientHost,$path,$openT,$size,$srvHost) = split('\t');
        }

        my $sessionId = &findSessionId($user,$pid,$clientHost,$srvHost,$siteName);
        next if ( ! $sessionId ); # error: no corresponding session id

        my $jobId = &runQueryWithRet("SELECT jobId
                                       FROM ${siteName}_openedSessions
                                      WHERE id = $sessionId ");
        if ( $version == 1 ) {
            $openT = &runQueryWithRet("SELECT CONVERT_TZ('$openT', '$timeZones{$siteName}', 'GMT') ");
        }
        
        &runQuery("UPDATE ${siteName}_jobs   
                      SET beginT = LEAST( '$openT', beginT)
                    WHERE jobId = $jobId");
        
        my $pathId = &findOrInsertPathId($path, $size);
        #print "$sessionId $pathId \n";
        if ( ! $pathId ) {
             print "path id not found for $path \n";
            next;
        }
        
        print MYSQLIN "$fileId \t $sessionId \t $pathId \t $openT \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_openedFiles");

    print "$rows rows loaded \n";
    `rm $mysqlIn $inFile`;

}

sub loadCloseFile() {
    my ($siteName) = @_;

    my $inFile = "$jrnlDir/$siteName/cfile-V${version}.ascii";
    if ( -z $inFile ) {return;}

    print "Loading closed files version $version ... \n";
    my $mysqlIn = "$jrnlDir/$siteName/mysqlin.c";
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
        my $jobId = &runQueryWithRet("SELECT jobId
                                       FROM ${siteName}_openedSessions
                                      WHERE id = $sessionId ");
        if ( ! $jobId ) {
             $jobId = &runQueryWithRet("SELECT jobId
                                          FROM ${siteName}_closedSessions
                                         WHERE id = $sessionId ");
        }
        if ( $version == 1 ) {
            $closeT = &runQueryWithRet("SELECT CONVERT_TZ('$closeT', '$timeZones{$siteName}', 'GMT') ");
        }

        &runQuery("UPDATE ${siteName}_jobs   
                      SET endT = GREATEST( '$closeT', endT)
                    WHERE jobId = $jobId");
        

        # remove it from the open files table
        &runQuery("DELETE FROM ${siteName}_openedFiles WHERE id = $fileId");

        # and insert into the closed
        print MYSQLIN "$fileId \t $sessionId \t $pathId \t  ";
        print MYSQLIN "$openT \t  $closeT \t $bytesR \t $bytesW \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE
               INTO TABLE ${siteName}_closedFiles         ");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastHour");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastDay");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastWeek");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastMonth");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastYear");
    
    print "$rows rows loaded \n";
    `rm $mysqlIn $inFile`;
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost, $siteName) = @_;

    my $userId       = findOrInsertUserId($user, $siteName);
    my $clientHostId = findOrInsertHostId($clientHost, $siteName);
    my $serverHostId = findOrInsertHostId($srvHost, $siteName);

    return &runQueryWithRet("SELECT id FROM ${siteName}_openedSessions 
                                       WHERE userId=$userId          AND
                                             pId=$pid                AND
                                             clientHId=$clientHostId AND
                                             serverHId=$serverHostId      ");
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
    my ($path, $size) = @_;

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
                   VALUES (\"$path\", $typeId, $skimId, $size, $hashValue )");
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

sub loadXrdRestarts() {
    my ($siteName, $version) = @_;

    my $siteId = $siteIds{$siteName};

    my $inFile = "$jrnlDir/$siteName/rfile-V$version}.ascii";
    if ( -z $inFile ) {return;}

    if ( !open INFILE, "<$inFile" ) {
        return;
    }
    while ( <INFILE> ) {
        chomp;
        my ($r, $hostName, $timestamp) = split('\t');

        my $hostId = $hostIds{$hostName};
        if ( ! $hostId ) {
            $hostId = &runQueryWithRet("SELECT id
                                        FROM ${siteName}_hosts
                                        WHERE hostName = \"$hostName\"");
	}
        if ( ! $hostId ) {
	    return;
	}
        &runQuery("INSERT IGNORE INTO xrdRestarts(hostId, siteId, startT)
                   VALUES ($hostId, $siteId, '$timestamp')");
    }
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
