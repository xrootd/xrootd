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
    } elsif ( $token =~ "site:" ) {
        if ( ! -e $v2 ) {
            print "Can't file file $v2\n";
	    close INFILE;
	    exit;
	}
	$siteInputFiles{$v1} = $v2;
    } else {
	print "Invalid entry: \"$_\"\n";
        close INFILE;
	exit;
    }
}
close INFILE;

# do the necessary one-time initialization
unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
    print "Error while connecting to database. $DBI::errstr\n";
    return;
}

@siteNames = &runQueryRetArray("SELECT name FROM sites");
foreach $siteName (@siteNames) {
    if ( ! $siteInputFiles{$siteName} ) {
	print "Config file does not specify location of input file ";
        print "for site \"$siteName\"\n";
	$dbh->disconnect();
	exit;
    }
    $siteIds{$siteName} = &runQueryWithRet("SELECT id 
                                            FROM sites 
                                            WHERE name = \"$siteName\"");
}

$dbh->disconnect();

my $stopFName = "$confFile.stop";
@primes = (101, 127, 157, 181, 199, 223, 239, 251, 271, 307);

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

sub doLoading {
    my $ts = &timestamp();

    # connect to the database
    print "\n$ts Connecting to database...\n";
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	return;
    }

    # load data for each site
    my $nr = 0;
    foreach $siteName (@siteNames) {
	$nr += loadOneSite($siteName, $ts);
    }

    # disconnect from db
    $dbh->disconnect();

    $ts = &timestamp();
    print "$ts All done, processed $nr entries.\n";

}

sub loadOneSite() {
    my ($siteName, $loadTime) = @_;

    print "Loading for --> $siteName <--\n";

    my $inFN   = $siteInputFiles{$siteName};
    my $siteId = $siteIds{$siteName};

    # lock the file
    unless ( $lockF = &lockTheFile($inFN) ) {
        return 0;
    }

    # open the input file for reading
    unless ( open INFILE, "< $inFN" ) {
	print "Can't open file $inFN for reading\n";
	return 0;
    }
    # read the file, sort the data, close the file
    open OFILE, ">/tmp/ofile.txt" or die "can't open ofile.txt for write: $!";
    open UFILE, ">/tmp/ufile.txt" or die "can't open ufile.txt for write: $!";
    open DFILE, ">/tmp/dfile.txt" or die "can't open dfile.txt for write: $!";
    open CFILE, ">/tmp/cfile.txt" or die "can't open cfile.txt for write: $!";
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
    # make a backup, remove the input file
    my $backupFName = "$inFN.backup";
    `touch $backupFName; cat $inFN >> $backupFName; rm $inFN`;
    # unlock the lock file
    unlockTheFile($lockF);
    
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
    my $mysqlIn = "/tmp/mysqlin.u";
    open INFILE, "</tmp/ufile.txt" or die "can't open ufile.txt for read: $!";
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
}


sub loadCloseSession() {
    my ($siteName) = @_;

    print "Loading closed sessions... \n";
    my $mysqlIn = "/tmp/mysqlin.d";
    open INFILE, "</tmp/dfile.txt" or die "can't open dfile.txt for read: $!";
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

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastHour");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastDay");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastWeek");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedSessions_LastMonth");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedSessions_2005");

    print "$rows rows loaded \n";
}


sub loadOpenFile() {
    my ($siteName) = @_;

    print "Loading opened files...\n";
    my $mysqlIn = "/tmp/mysqlin.o";
    open INFILE, "</tmp/ofile.txt" or die "can't open ofile.txt for read: $!";
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

        my $pathId = findOrInsertPathId($path, $siteName);
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

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_openedFiles");
    print "$rows rows loaded \n";

}

sub loadCloseFile() {
    my ($siteName) = @_;

    print "Loading closed files... \n";
    my $mysqlIn = "/tmp/mysqlin.c";
    open INFILE, "</tmp/cfile.txt" or die "can't open cfile.txt for read: $!";
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

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastHour");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastDay");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastWeek");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_LastMonth");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${siteName}_closedFiles_2005");
    print "$rows rows loaded \n";
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
    my ($path, $siteName) = @_;

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
    my $sec   = (localtime)[0];
    my $min   = (localtime)[1];
    my $hour  = (localtime)[2];
    my $day   = (localtime)[3];
    my $month = (localtime)[4] + 1;
    my $year  = (localtime)[5] + 1900;

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
