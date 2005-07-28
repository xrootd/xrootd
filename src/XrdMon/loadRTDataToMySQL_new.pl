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
if ( @ARGV!=1) {
    print "Expected argument <configFile>\n";
    exit;
}
$confFile = $ARGV[0];
unless ( open INFILE, "< $confFile" ) {
    print "Can't open file $confFile\n";
    exit;
}

while ($_ = <INFILE> ) {
    chomp();
    my ($token, $v1, $v2) = split(/ /, $_);
    if ( $token =~ "dbName:" ) {
	$dbName = $v1;
    } elsif ( $token =~ "MySQLUser:" ) {
	$mySQLUser = $v1;
    } elsif ( $token =~ "updateInterval:" ) {
	$updInt = $v1;
    } elsif ( $token =~ "site:" ) {
        if ( ! -e $v2 ) {
            print "Can't file file $v2\n";
	    exit;
	}
	$siteInputFiles{$v1} = $v2;
    } else {
	print "Invalid entry: \"$_\"\n";
	exit;
    }
}


$initFlag = 1;

# prepare counters for updating top performers tables
                      # update data for top perf 1 hour  period every  1 min
$dUpdatesFreq =   30; # update data for top perf 1 day   period every 30 min
$wUpdatesFreq =  185; # update data for top perf 1 week  period every  3+ hours
$mUpdatesFreq = 1452; # update data for top perf 1 month period every 24+ hours
$yUpdatesFreq = 4327; # update data for top perf 1 year  period every  3+ days
$dCounter  = $dUpdatesFreq;
$wCounter  = $wUpdatesFreq;
$mCounter  = $mUpdatesFreq;
$yCounter  = $yUpdatesFreq;

$loadFreq1stFail =   60;  # File size Loading frequency for files that have failed once,    every hour.
$loadFreq2ndFail =  720;  # File size Loading frequency for files that have failed twice,   every 12 hours.
$loadFreq3rdFail = 1440;  # File size Loading frequency for files that have failed 3 times, every 24 hour.


my $stopFName = "$confFile.stop";
$cycleEndTime = time() + $updInt;
 
#start an infinite loop
while ( 1 ) {
    &doLoading();
    print "sleeping ... \n";
    # sleep in 5 sec intervals, catch "stop" signal in before each sleep
    $noSleeps = $timeLeft > 0 ? $timeLeft / 5 : 0;
    for ( $i=0 ; $i<=$noSleeps ; $i++) {
        if ( -e $stopFName ) {
            unlink $stopFName;
            exit;
	}
        sleep 5;
    }
    if ( $timeLeft > 0 ) {sleep ( $timeLeft % 5 );}
    for ($i=0;$cycleEndTime <= time(); $i++) {
        $cycleEndTime += $updInt;
        if ( $i > 0 ) { 
             print "WARNING: update required $i extra unit(s) \n";
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

    # do initilization in the first connection
    if ( $initFlag ) { &doInit(); }

    # set the load time.
    my $loadTime = &timestamp();

    # $nMin, $nHour, $nwDay and $nDay start from 0
    my $loadLastHour  = 1;    # always load "last hour"
    my $loadLastDay   = 0;
    my $loadLastWeek  = 0;
    my $loadLastMonth = 0;
    my $loadAllMonths = 0;

    if ( $nMin % 60 == 59 ) {
        $loadLastDay = 1;
	if ( $nWDay % 7 == 6 ) {
	    $loadLastWeek = 1;
	    $nWDay += 1;
	}
        if ( $nHour % 24 == 23 ) {
	    $loadLastMonth = 1;
            $nDay += 1;
            if ( (localtime)[3] == 1 ) {
		$loadAllMonths = 1;
	    }
        }
        $nHour += 1;
    }
    $nMin += 1;

    @sites = &runQueryRetArray("SELECT name FROM sites");
    foreach $site (@sites) {
	if ( ! $siteInputFiles{$site} ) {
            print "Config file does not specify location of input file for site \"$site\"\n";
	    exit;
	}
    }

    my $nr = 0;
    foreach $site (@sites) {
	$nr += loadOneSite($site, 
			   $loadTime,
			   $loadLastHour,
			   $loadLastDay,
			   $loadLastWeek,
			   $loadLastMonth,
			   $loadAllMonths
			   );
    }

    # disconnect from db
    $dbh->disconnect();

    $ts = &timestamp();
    print "$ts All done, processed $nr entries.\n";

}

sub loadOneSite() {
    my ($siteName, $loadTime, 
        $loadLastHour, $loadLastDay, 
        $loadLastWeek, $loadLastMonth, $loadAllMonths) = @_;

    print "Loading for --> $site <--\n";

    my $inFN = $siteInputFiles{$site};

    $siteId = &runQueryWithRet("SELECT id FROM sites WHERE name = \"$siteName\"");
    # lock the file
    unless ( $lockF = &lockTheFile($inFN) ) {
	return -1;
    }

    # open the input file for reading
    unless ( open INFILE, "< $inFN" ) {
	print "Can't open file $inFName for reading\n";
	&unlockTheFile($lockF);
	return -1;
    }
    # read the file, sort the data, close the file
    open OFILE, ">/tmp/ofile.txt" or die "can't open ofile.txt for writing: $!";
    open UFILE, ">/tmp/ufile.txt" or die "can't open ufile.txt for writing: $!";
    open DFILE, ">/tmp/dfile.txt" or die "can't open dfile.txt for writing: $!";
    open CFILE, ">/tmp/cfile.txt" or die "can't open cfile.txt for writing: $!";
    print "Sorting...\n";
    my $nr = 0;
    while ( <INFILE> ) {
        if ( $_ =~ m/^u/ ) { print UFILE $_ ;  }
        if ( $_ =~ m/^d/ ) { print DFILE $_ ;  }
        if ( $_ =~ m/^o/ ) { print OFILE $_ ;  }
        if ( $_ =~ m/^c/ ) { print CFILE $_ ;  }
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
    &loadOpenSession($site);
    &loadOpenFile($site);
    &loadCloseSession($site);
    &loadCloseFile($site);
    
    if ( $loadLastHour  == 1 ) { &loadStatsLastHour($site, $loadTime)  };
    if ( $loadLastDay   == 1 ) { &loadStatsLastDay($site, $loadTime)   };
    if ( $loadLastWeek  == 1 ) { &loadStatsLastWeek($site, $loadTime)  };
    if ( $loadLastMonth == 1 ) { &loadStatsLastMonth($site, $loadTime) };
    if ( $loadAllMonths == 1 ) { &loadStatsAllMonths($site, $loadTime) };

    &reloadTopPerfTables($site);

    # load file sizes
    if       ( ! ($nMin % $loadFreq3rdFail) ) {
        &loadFileSizes( $site, -3 );
    } elsif ( ! ($nMin % $loadFreq2ndFail) ) {
        &loadFileSizes( $site, -2 );
    } elsif ( ! ($nMin % $loadFreq1stFail) ) {
        &loadFileSizes( $site, -1 );
    } else {
        &loadFileSizes( $site, 0 ); # every minute
    }
    return $nr;
}

# opens the <fName>.lock file for writing & locks it (write lock)
sub lockTheFile() {
    my ($fName) = @_;

    $lockFName = "$fName.lock";
    print "Locking $lockFName...\n";
    unless ( open($lockF, "> $lockFName") ) {
	print "Can't open file $inFName 4 writing\n";
	return;
    }
    $lk_parms = pack('sslllll', F_WRLCK, 0, 0, 0, 0, 0, 0);
    fcntl($lockF, F_SETLKW, $lk_parms) or die "can't fcntl F_SETLKW: $!";
    return $lockF;
}

sub unlockTheFile() {
    my ($fh) = @_;
    $lk_parms = pack('sslllll', F_UNLCK, 0, 0, 0, 0, 0, 0);
    fcntl($fh, F_SETLKW, $lk_parms);
}

sub loadOpenSession() {
    my ($site) = @_;

    print "Loading open sessions...\n";
    my $mysqlIn = "/tmp/mysqlin.u";
    open INFILE, "</tmp/ufile.txt" or die "can't open ufile.txt for reading: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;
        my ($u, $sessionId, $user, $pid, $clientHost, $srvHost) = split('\t');
        #print "u=$u, id=$id, user=$user, pid=$pid, ch=$clientHost, sh=$srvHost\n";

        my $userId       = findOrInsertUserId($user, $site);
        my $clientHostId = findOrInsertHostId($clientHost, $site);
        my $serverHostId = findOrInsertHostId($srvHost, $site);

        #print "uid=$userId, chid=$clientHostId, shd=$serverHostId\n";
        print MYSQLIN "$sessionId \t  $userId \t $pid \t $clientHostId \t $serverHostId \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_openedSessions");
    print "... $rows rows loaded \n";
}


sub loadCloseSession() {
    my ($site) = @_;

    print "Loading closed sessions... \n";
    my $mysqlIn = "/tmp/mysqlin.d";
    open INFILE, "</tmp/dfile.txt" or die "can't open dfile.txt for reading: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($d, $sessionId, $sec, $timestamp) = split('\t');
        #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";


        # find if there is corresponding open session, if not don't bother
        my ($userId, $pId, $clientHId, $serverHId) = 
	   &runQueryWithRet("SELECT userId, pId, clientHId, serverHId FROM ${site}_openedSessions WHERE id = $sessionId");
        next if ( ! $pId  );

        #print "received decent data for sId $sessionId: uid=$userId, pid = $pId, cId=$clientHId, sId=$serverHId\n";

        # remove it from the open session table
        &runQuery("DELETE FROM ${site}_openedSessions WHERE id = $sessionId");

        # and insert into the closed
        print MYSQLIN "$sessionId \t $userId \t $pId \t $clientHId \t $serverHId \t $sec \t $timestamp \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedSessions_LastHour");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedSessions_LastDay");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedSessions_LastWeek");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedSessions_LastMonth");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedSessions_2005");

    print "$rows rows loaded \n";
}


sub loadOpenFile() {
    my ($site) = @_;

    print "Loading opened files...\n";
    my $mysqlIn = "/tmp/mysqlin.o";
    open INFILE, "</tmp/ofile.txt" or die "can't open ofile.txt for reading: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($o, $fileId, $user, $pid, $clientHost, $path, $openTime, $srvHost) = 
	    split('\t');
        #print "\no=$o, id=$id, user=$user, pid=$pid, ch=$clientHost, p=$path, ";
        #print "time=$openTime, srvh=$srvHost\n";

        my $sessionId = findSessionId($user, $pid, $clientHost, $srvHost, $site);
        if ( ! $sessionId ) {
	    #print "session id not found for $user $pid $clientHost $srvHost\n";
	    next; # error: no corresponding session id
        }

        my $pathId = findOrInsertPathId($path, $site);
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
               INTO TABLE ${site}_openedFiles");
    print "$rows rows loaded \n";

}

sub loadCloseFile() {
    my ($site) = @_;

    print "Loading closed files... \n";
    my $mysqlIn = "/tmp/mysqlin.c";
    open INFILE, "</tmp/cfile.txt" or die "can't open cfile.txt for reading: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t');
        #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

        # find if there is corresponding open file, if not don't bother
        my ($sessionId, $pathId, $openT) = 
	    &runQueryWithRet("SELECT sessionId, pathId, openT FROM ${site}_openedFiles WHERE id = $fileId");
        next if ( ! $sessionId );

        # remove it from the open files table
        &runQuery("DELETE FROM ${site}_openedFiles WHERE id = $fileId");

        # and insert into the closed
        print MYSQLIN "$fileId \t $sessionId \t $pathId \t  $openT \t  $closeT \t $bytesR \t $bytesW \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedFiles_LastHour");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedFiles_LastDay");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedFiles_LastWeek");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedFiles_LastMonth");
    &runQuery("LOAD DATA LOCAL INFILE \"$mysqlIn\" IGNORE 
               INTO TABLE ${site}_closedFiles_2005");
    print "$rows rows loaded \n";
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost, $site) = @_;

    my $userId       = findOrInsertUserId($user, $site);
    my $clientHostId = findOrInsertHostId($clientHost, $site);
    my $serverHostId = findOrInsertHostId($srvHost, $site);

    return &runQueryWithRet("SELECT id FROM ${site}_openedSessions 
                                       WHERE     userId=$userId 
                                             AND pId=$pid 
                                             AND clientHId=$clientHostId 
                                             AND serverHId=$serverHostId");
}


sub findOrInsertUserId() {
    my ($userName, $site) = @_;

    my $userId = $userIds{$userName};
    if ( $userId ) {
        return $userId;
    }
    $userId = &runQueryWithRet("SELECT id FROM ${site}_users WHERE name = \"$userName\"");
    if ( $userId ) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO ${site}_users (name) VALUES (\"$userName\")");

	$userId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
    }
    $userIds{$userName} = $userId;
    return $userId;
}

sub findOrInsertHostId() {
    my ($hostName, $site) = @_;

    my $hostId = $hostIds{$hostName};
    if ( $hostId ) {
        return $hostId;
    }
    $hostId = &runQueryWithRet("SELECT id FROM ${site}_hosts WHERE hostName = \"$hostName\"");
    if ( $hostId ) {
	#print "Will reuse hostId $clientHostId for $hostName\n";
    } else {
	#print "$hostName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO ${site}_hosts (hostName) VALUES (\"$hostName\")");

	$hostId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
    }
    $hostIds{$hostName} = $hostId;
    return $hostId;
}

sub findOrInsertPathId() {
    my ($path, $site) = @_;

    use vars qw($pathId $typeId $skimId);
                                                                                                      
    $pathId = $pathIds{$path};
    if ( $pathId ) {
        #print "from cache: $pathId for $path\n";
        return $pathId;
    }
    my $hashValue = &returnHash("$path");
    ($pathId, $typeId, $skimId) =
        &runQueryWithRet("SELECT id, typeId, skimId FROM ${site}_paths 
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
            $typeId = &runQueryWithRet("SELECT id FROM fileTypes WHERE name = \"$typeName\"");
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
                $skimId = &runQueryWithRet("SELECT id FROM skimNames WHERE name = \"$skimName\"");
            }
            if ( ! $skimId ) {
                &runQuery("INSERT INTO skimNames(name) VALUES(\"$skimName\") ");
                $skimId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
            }
        }
        &runQuery("INSERT INTO ${site}_paths (name, typeId, skimId, size, hash) 
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


sub printHelp() {

  print "loadRTDataToMySQL\n";
  print "    [-donotload]\n";
  print "\n";
  print " -donotload    Turns off loading data to MySQL. MySQL script is produced.\n";
  print "\n";
}


sub doInit() {
    my ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastHour");
    if ( $lastTime ) {
        ($nMin, $lastNoJobs, $lastNoUsers, $lastNoUniqueF, $lastNoNonUniqueF) 
            = &runQueryWithRet("SELECT seqNo, noJobs, noUsers, noUniqueF, noNonUniqueF
                                FROM   statsLastHour 
                                WHERE  date = \"$lastTime\"");
        $nMin += 1;
    } else { 
        $nMin = $lastNoJobs = $lastNoUsers = $lastNoUniqueF = $lastNoNonUniqueF = 0;
    }

    ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastDay");
    if ( $lastTime ) {
        $nHour = &runQueryWithRet("SELECT seqNo FROM statsLastDay WHERE date = \"$lastTime\"");
        $nHour += 1;
    } else { 
        $nHour = 0;
    }
    ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastWeek");
    if ( $lastTime ) {
        $nWDay = &runQueryWithRet("SELECT seqNo FROM statsLastWeek WHERE date = \"$lastTime\"");
        $nWDay += 1;
    } else { 
        $nWDay = 0;
    }

    ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastMonth");
    if ( $lastTime ) {
        $nDay = &runQueryWithRet("SELECT seqNo FROM statsLastMonth WHERE date = \"$lastTime\"");
        $nDay += 1;
    } else {
        $nDay = 0;
    }
    @primes = (101, 127, 157, 181, 199, 223, 239, 251, 271, 307);
    $bbkListSize = 250;
    $minSizeLoadTime = 5;
    $initFlag = 0;
}

sub timestamp() {
    my $sec  = (localtime)[0];
    my $min  = (localtime)[1];
    my $hour = (localtime)[2];
    my $day  = (localtime)[3];
    my $month  = (localtime)[4] + 1;
    my $year = (localtime)[5] + 1900;

    return sprintf("%04d-%02d-%02d %02d:%02d:%02d", $year, $month, $day, $hour, $min, $sec);
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


sub loadFileSizes() {

    print "Loading file sizes... \n";
    use vars qw($sizeIndex $fromId $toId $path $size @files @inBbk);
    my ($site, $sizeIndex) = @_;
    &runQuery("CREATE TEMPORARY TABLE zerosize  (name VARCHAR(255), id MEDIUMINT UNSIGNED, hash MEDIUMINT)");
    &runQuery("INSERT INTO zerosize(name, id, hash) SELECT name, id, hash FROM ${site}_paths 
                                              WHERE size BETWEEN $sizeIndex AND 0 
                                              ORDER BY id 
                                              LIMIT 7500");
     
    $skip = 0;
    while () {
       my $t0 = time();
       $timeLeft = $cycleEndTime - $t0;
       last if ( $timeLeft < $minSizeLoadTime);
       @files = &runQueryRetArray("SELECT name FROM zerosize LIMIT $skip, $bbkListSize "); 
       #print scalar @files, "\n";
       last if ( ! @files );

       open ( BBKINPUT, '>bbkInput' ) or die "Can't open bbkInput file: $!"; 
       my $index = 0;
       while ( defined $files[$index] ) {
           print BBKINPUT "$files[$index]\n";
           $index++;
       }
       @bbkOut = `BbkUser --lfn-file=bbkInput --quiet lfn bytes`;
       @inBbk = ();
       while ( @bbkOut ) {
           $line = shift @bbkOut;
           chomp $line;
           ($path, $size) = split (' ', $line);
           @inBbk = (@inBbk, $path);
           my $hashValue = &returnHash("$path");
           my $id = &runQueryWithRet("SELECT id FROM zerosize 
                                      WHERE hash = $hashValue AND name = '$path'");
           &runQuery("UPDATE ${site}_paths SET size = $size WHERE id = $id ");
       }
       # decrement size by 1 for files that failed bbk.
       foreach $path ( @files ) {
           if ( ! grep { $_ eq $path } @inBbk ) {
               my $hashValue = &returnHash("$path");
               my $id = &runQueryWithRet("SELECT id FROM zerosize 
                                          WHERE hash = $hashValue AND name = '$path'");
               &runQuery("UPDATE ${site}_paths SET size = size - 1 WHERE id = $id ");
           }
       }
       print " done ", scalar @files, " files updated. Update time = ", time() - $t0, " s \n";
       last if ( @files < $bbkListSize );
       $skip += $bbkListSize;
    }
    &runQuery("DROP TABLE IF EXISTS zerosize");
}


sub loadStatsLastHour() {
    my ($site, $loadTime) = @_;

    use vars qw($seqNo $noJobs $noUsers $noUniqueF $noNonUniqueF $deltaJobs $jobs_p 
                $deltaUsers $users_p $deltaUniqueF $uniqueF_p $deltaNonUniqueF $nonUniqueF_p);
    $seqNo = $nMin % 60;
    &runQuery("DELETE FROM statsLastHour WHERE seqNo = $seqNo AND siteId = $siteId");
    ($noJobs, $noUsers) = &runQueryWithRet("SELECT COUNT(DISTINCT pId, clientHId), COUNT(DISTINCT userId) 
                                                FROM ${site}_openedSessions");

    ($noUniqueF, $noNonUniqueF) = &runQueryWithRet("SELECT COUNT(DISTINCT pathId), COUNT(*) 
                                                        FROM ${site}_openedFiles");
    &runQuery("INSERT INTO statsLastHour 
                          (seqNo, siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF) 
                   VALUES ($seqNo, $siteId, \"$loadTime\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF)");

    &runQuery("TRUNCATE TABLE rtChanges");
    $deltaJobs = $noJobs - $lastNoJobs; 
    $jobs_p = $lastNoJobs > 0 ? &roundoff( 100 * $deltaJobs / $lastNoJobs ) : -1;
    $deltaUsers = $noUsers - $lastNoUsers;
    $users_p = $lastNoUsers > 0 ? &roundoff( 100 * $deltaUsers / $lastNoUsers ) : -1;
    $deltaUniqueF = $noUniqueF - $lastNoUniqueF;
    $uniqueF_p = $lastNoUniqueF > 0 ? &roundoff( 100 * $deltaUniqueF / $lastNoUniqueF ) : -1;
    $deltaNonUniqueF = $noNonUniqueF - $lastNoNonUniqueF;
    $nonUniqueF_p = $lastNoNonUniqueF > 0 ? &roundoff( 100 * $deltaNonUniqueF / $lastNoNonUniqueF ) : -1;
    &runQuery("INSERT INTO rtChanges 
                          (siteId, jobs, jobs_p, users, users_p, uniqueF, uniqueF_p, 
                           nonUniqueF, nonUniqueF_p, lastUpdate)
                   VALUES ($siteId, $deltaJobs, $jobs_p, $deltaUsers, $users_p, $deltaUniqueF,
                           $uniqueF_p, $deltaNonUniqueF, $nonUniqueF_p, \"$loadTime\")");
    $lastNoJobs = $noJobs;
    $lastNoUsers = $noUsers;
    $lastNoUniqueF = $noUniqueF;
    $lastNoNonUniqueF = $noNonUniqueF;
}

sub loadStatsLastDay() {
    my ($site, $loadTime) = @_;

    my $seqNo = $nHour % 24;
    &runQuery("DELETE FROM statsLastDay WHERE seqNo = $seqNo AND siteId = $siteId");

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastHour WHERE siteId = $siteId");

    &runQuery("INSERT INTO statsLastDay 
                          (seqNo, siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                   VALUES ($seqNo, $siteId, \"$loadTime\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
                           $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                           $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");
}

sub loadStatsLastWeek() {
    my ($site, $loadTime) = @_;
    print "called loadStatsLastWeek\n";
    my $seqNo = $nWDay % 7;
    &runQuery("DELETE FROM statsLastWeek WHERE seqNo = $seqNo AND siteId = $siteId");

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastDay WHERE siteId = $siteId");

    &runQuery("INSERT INTO statsLastWeek 
                          (seqNo, siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                   VALUES ($seqNo, $siteId, \"$loadTime\", $noJobs, $noUsers,
                           $noUniqueF, $noNonUniqueF, 
                           $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                           $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");
}

sub loadStatsLastMonth() {
    my ($site, $loadTime) = @_;

    my $seqNo = $nDay % 31;
    &runQuery("DELETE FROM statsLastMonth WHERE seqNo = $seqNo AND siteId = $siteId");

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastDay WHERE siteId = $siteId");

    &runQuery("INSERT INTO statsLastMonth 
                          (seqNo, siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                    VALUES ($seqNo, $siteId, \"$loadTime\", $noJobs, $noUsers,
                            $noUniqueF, $noNonUniqueF, 
                            $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                            $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");
}

sub loadStatsAllMonths() {
    my ($site, $loadTime) = @_;

    # note that (localtime)[4] returns month in the range 0 - 11 and normally should be
    # increased by 1 to show the current month.
    $lastMonth = (localtime)[4];

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastMonth WHERE MNTH(date) = \"$lastMonth\" AND siteId = $siteId"); 

    &runQuery("INSERT INTO statsAllMonths 
                          (siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                    VALUES ($siteId, \"$loadTime\", $noJobs, $noUsers,
                            $noUniqueF, $noNonUniqueF, 
                            $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                            $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");
}


sub roundoff() {
   my $a = shift;
   $d = 0;
   if ( $a < 10 ) {$d = $a < 1 ? 2 : 1;}
   return sprintf("%.${d}f", $a);
}






#######################################################
### everything below is for loading top perf tables ###
#######################################################


sub reloadTopPerfTables() {
    my ($site) = @_;

    # create tables
    &runQuery("CREATE TEMPORARY TABLE jj  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE ff  (theId INT, n INT, s INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE uu  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE vv  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE xx  (theId INT UNIQUE KEY, INDEX (theId))");
    @tables = ("jj", "ff", "uu", "vv", "xx");

    # run queries for past (last hour/day/week/month/year
    &runQueries4AllTopPerfTablesPast("Hour", 20, $site);
    if ( $dCounter == $dUpdatesFreq ) {
#	&runQueries4AllTopPerfTablesPast("Day", 20, $site);
	$dCounter = 0;
    } else {
	$dCounter += 1;
    }
    if ( $wCounter == $wUpdatesFreq ) {
#	&runQueries4AllTopPerfTablesPast("Week", 20, $site);
	$wCounter = 0;
    } else {
	$wCounter += 1;
    }
    if ( $mCounter == $mUpdatesFreq ) {
#	&runQueries4AllTopPerfTablesPast("Month", 20, $site);
	$mCounter = 0;
    } else {
	$mCounter += 1;
    }
    if ( $yCounter == $yUpdatesFreq ) {
#	&runQueries4AllTopPerfTablesPast("Year", 20, $site);
	$yCounter = 0;
    } else {
	$yCounter += 1;
    }
    # run queries for now
    &runQueries4AllTopPerfTablesNow(20, $site);

    # delete tables
    foreach $table (@tables) { &runQuery("DROP TABLE IF EXISTS $table"); }
}

sub runQueries4AllTopPerfTablesPast() {
    my ($theKeyword, $theLimit, $site) = @_;

    &runTopUsrFsQueriesPast($theKeyword, $theLimit, "USERS", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesPast($theKeyword, $theLimit, "SKIMS", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesPast($theKeyword, $theLimit, "TYPES", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopUsrFsQueriesPast($theKeyword, $theLimit, "FILES", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
}

sub runQueries4AllTopPerfTablesNow() {
    my ($theLimit, $site) = @_;

    &runTopUsrFsQueriesNow($theLimit, "USERS", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesNow($theLimit, "SKIMS", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesNow($theLimit, "TYPES", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopUsrFsQueriesNow($theLimit, "FILES", $site);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
}

sub runTopUsrFsQueriesPast() {
    my ($theKeyword, $theLimit, $what, $site) = @_;

    print "# updating topPerf $what tables for $theKeyword\n";

    my $idInTable        = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "USERS" ) {
        $idInTable        = "userId";
        $destinationTable = "${site}_topPerfUsersPast";
    } elsif ( $what eq "FILES" ) {
        $idInTable        = "pathId";
        $destinationTable = "${site}_topPerfFilesPast";
    } else {
        die "Invalid arg, expected USERS or FILES\n";
    }

    # past jobs
    if ( $what eq "USERS" ) {
        &runQuery("INSERT INTO jj
            SELECT $idInTable, 
                   COUNT(DISTINCT pId, clientHId) AS n
            FROM   ${site}_closedSessions_Last$theKeyword
            GROUP BY $idInTable");
    } elsif ( $what eq "FILES" ) {
        &runQuery("INSERT INTO jj
            SELECT $idInTable, 
                   COUNT(DISTINCT pId, clientHId) AS n
            FROM   ${site}_closedSessions_Last$theKeyword cs, ${site}_closedFiles_Last$theKeyword cf
            WHERE  cs.id = cf.sessionId
            GROUP BY $idInTable");
    }
    if ( $what eq "USERS" ) {
	# past files - through opened & closed sessions
	&runQuery("INSERT INTO ff           
           SELECT tmp.$idInTable, 
                  COUNT(tmp.pathId),
                  SUM(tmp.size)/(1024*1024)
           FROM   ( SELECT DISTINCT oc.$idInTable, oc.pathId, oc.size
                    FROM   ( SELECT $idInTable, pathId, size 
                             FROM   ${site}_openedSessions os,
                                    ${site}_closedFiles_Last$theKeyword cf,
                                    ${site}_paths p
                             WHERE  os.id = cf.sessionId 
                                AND cf.pathId = p.id 
                             UNION ALL
                             SELECT $idInTable, pathId, size 
                             FROM   ${site}_closedSessions_Last$theKeyword cs,
                                    ${site}_closedFiles_Last$theKeyword cf,
                                    ${site}_paths p
                             WHERE  cs.id = cf.sessionId 
                                AND cf.pathId = p.id 
                            ) AS oc
                   ) AS tmp
           GROUP BY tmp.$idInTable");
    }
    # past volume - through opened & closed sessions
    &runQuery("INSERT INTO vv
        SELECT oc.$idInTable, 
               SUM(oc.bytesR)/(1024*1024)
        FROM   ( SELECT $idInTable, bytesR
                 FROM  ${site}_openedSessions os, ${site}_closedFiles_Last$theKeyword cf
                 WHERE os.id = cf.sessionId
                 UNION ALL
                 SELECT $idInTable, bytesR
                 FROM  ${site}_closedSessions_Last$theKeyword cs, ${site}_closedFiles_Last$theKeyword cf
                 WHERE cs.id = cf.sessionId
               ) AS oc
       GROUP BY oc.$idInTable");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM vv ORDER BY n DESC LIMIT $theLimit");

    ## delete old data
    &runQuery("DELETE FROM $destinationTable WHERE timePeriod LIKE \"$theKeyword\"");

    ## and finally insert the new data
    if ( $what eq "USERS" ) {
        &runQuery("INSERT INTO $destinationTable
            SELECT xx.theId, 
                   IFNULL(jj.n, 0) AS jobs, 
                   IFNULL(ff.n, 0) AS files, 
                   IFNULL(ff.s, 0) AS fSize, 
                   IFNULL(vv.n, 0) AS vol, 
                   \"$theKeyword\"
            FROM   xx 
                   LEFT OUTER JOIN jj ON xx.theId = jj.theId
                   LEFT OUTER JOIN ff ON xx.theId = ff.theId
                   LEFT OUTER JOIN vv ON xx.theId = vv.theId");
    } else {
        &runQuery("INSERT INTO $destinationTable
            SELECT xx.theId, 
                   IFNULL(jj.n, 0) AS jobs,
                   IFNULL(ff.s, 0) AS fSize,
                   IFNULL(vv.n, 0) AS vol, 
                   \"$theKeyword\"
            FROM   xx 
                   LEFT OUTER JOIN jj ON xx.theId = jj.theId
                   LEFT OUTER JOIN ff ON xx.theId = ff.theId
                   LEFT OUTER JOIN vv ON xx.theId = vv.theId");
    }
}


sub runTopUsrFsQueriesNow() {
    my ($theLimit, $what, $site) = @_;

    print "# updating topPerf $what tables for NOW\n";

    my $idInTable        = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "USERS" ) {
        $idInTable        = "userId";
        $destinationTable = "${site}_topPerfUsersNow";
        $pastTable        = "${site}_topPerfUsersPast";
    } elsif ( $what eq "FILES" ) {
        $idInTable        = "pathId";
        $destinationTable = "${site}_topPerfFilesNow";
        $pastTable        = "${site}_topPerfFilesPast";
    } else {
        die "Invalid arg, expected USERS or FILES\n";
    }

    if ( $what eq "USERS" ) {
        # now jobs
        &runQuery("INSERT INTO jj
           SELECT $idInTable, COUNT(DISTINCT pId, clientHId ) AS n
        FROM   ${site}_openedSessions
        GROUP BY $idInTable");

        # now files
        &runQuery ("INSERT INTO ff 
        SELECT tmp.$idInTable, 
               COUNT(tmp.pathId) AS n,
               SUM(tmp.size)/(1024*1024) AS s
        FROM   (SELECT DISTINCT $idInTable, pathId, size
                FROM   ${site}_openedSessions os, ${site}_openedFiles of, ${site}_paths p
                WHERE  os.id = of.sessionId
                   AND of.pathId = p.id
               ) AS tmp
        GROUP BY tmp.$idInTable");
    }
    if ( $what eq "FILES" ) {
        # now jobs
        &runQuery("INSERT INTO jj
            SELECT $idInTable, COUNT(DISTINCT pId, clientHId ) AS n
            FROM   ${site}_openedSessions os, ${site}_openedFiles of
            WHERE  os.id = of.sessionId
            GROUP BY $idInTable");

    }

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM $pastTable");
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");

    &runQuery("TRUNCATE TABLE $destinationTable");

    ## and finally insert the new data
    if ( $what eq "USERS" ) {
        &runQuery("INSERT INTO $destinationTable
            SELECT DISTINCT xx.theId,
                   IFNULL(jj.n, 0) AS jobs,
                   IFNULL(ff.n, 0) AS files, 
                   IFNULL(ff.s, 0) AS fSize
             FROM  xx 
                   LEFT OUTER JOIN jj ON xx.theId = jj.theId
                   LEFT OUTER JOIN ff ON xx.theId = ff.theId");
    } else {
        &runQuery("INSERT INTO $destinationTable
            SELECT DISTINCT xx.theId,
                   IFNULL(jj.n, 0) AS jobs,
                   IFNULL(ff.s, 0) AS fSize
             FROM  xx 
                   LEFT OUTER JOIN jj ON xx.theId = jj.theId
                   LEFT OUTER JOIN ff ON xx.theId = ff.theId");
    }
}

  
sub runTopSkimsQueriesPast() {
    my ($theKeyword, $theLimit, $what, $site) = @_;

    print "# updating topPerf $what tables for $theKeyword\n";

    my $idInPathTable    = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "SKIMS" ) {
        $idInPathTable    = "skimId";
        $destinationTable = "${site}_topPerfSkimsPast";
    } elsif ( $what eq "TYPES" ) {
        $idInPathTable    = "typeId";
        $destinationTable = "${site}_topPerfTypesPast";
    } else {
        die "Invalid arg, expected SKIMS or TYPES\n";
    }

    # past jobs
    &runQuery("REPLACE INTO jj
        SELECT $idInPathTable, 
               COUNT(DISTINCT pId, clientHId ) AS n
        FROM   ${site}_closedSessions_Last$theKeyword cs,
               ${site}_closedFiles_Last$theKeyword cf,
               ${site}_paths p
        WHERE  cs.id = cf.sessionId
           AND cf.pathId = p.id
        GROUP BY $idInPathTable");

    # past files - through opened & closed sessions
    &runQuery("INSERT INTO ff 
        SELECT tmp.$idInPathTable,
               COUNT(tmp.pathId),
               SUM(tmp.size)/(1024*1024)
        FROM   ( SELECT DISTINCT oc.$idInPathTable, oc.pathId, oc.size
               FROM   ( SELECT $idInPathTable, pathId, size
                        FROM   ${site}_openedSessions os,
                               ${site}_closedFiles_Last$theKeyword cf,
                               ${site}_paths p
                        WHERE  os.id = cf.sessionId
                           AND cf.pathId = p.id
                        UNION ALL
                        SELECT $idInPathTable, pathId, size
                        FROM   ${site}_closedSessions_Last$theKeyword cs,
                               ${site}_closedFiles_Last$theKeyword cf,
                               ${site}_paths p
                        WHERE  cs.id = cf.sessionId
                           AND cf.pathId = p.id
                      ) AS oc
                ) AS tmp
        GROUP BY tmp.$idInPathTable");



    # past users
    &runQuery("REPLACE INTO uu
        SELECT $idInPathTable, 
               COUNT(DISTINCT userId) AS n
        FROM   ${site}_closedSessions_Last$theKeyword cs,
               ${site}_closedFiles_Last$theKeyword cf,
               ${site}_paths p
        WHERE  cs.id = cf.sessionId
          AND cf.pathId = p.id
        GROUP BY $idInPathTable");

    # past volume - through opened & closed sessions
    &runQuery("INSERT INTO vv
         SELECT oc.$idInPathTable, 
                SUM(oc.bytesR)/(1024*1024)
         FROM   ( SELECT $idInPathTable, bytesR
                  FROM   ${site}_openedSessions os,
                         ${site}_closedFiles_Last$theKeyword cf,
                         ${site}_paths p
                  WHERE  os.id = cf.sessionId
                    AND cf.pathId = p.id
                  UNION ALL
                  SELECT $idInPathTable, bytesR
                  FROM   ${site}_closedSessions_Last$theKeyword cs,
                         ${site}_closedFiles_Last$theKeyword cf,
                         ${site}_paths p
                  WHERE  cs.id = cf.sessionId
                     AND cf.pathId = p.id
                ) AS oc
         GROUP BY oc.$idInPathTable");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM uu ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM vv ORDER BY n DESC LIMIT $theLimit");

    ## delete old data
    &runQuery("DELETE FROM $destinationTable WHERE timePeriod LIKE \"$theKeyword\"");

    ## and finally insert the new data
    &runQuery("INSERT INTO $destinationTable
        SELECT xx.theId,
               IFNULL(jj.n, 0) AS jobs,
               IFNULL(ff.n, 0) AS files,
               IFNULL(ff.s, 0) AS fSize,
               IFNULL(uu.n, 0) AS users, 
               IFNULL(vv.n, 0) AS vol, 
               \"$theKeyword\"
        FROM   xx 
               LEFT OUTER JOIN jj ON xx.theId = jj.theId
               LEFT OUTER JOIN ff ON xx.theId = ff.theId
               LEFT OUTER JOIN uu ON xx.theId = uu.theId
               LEFT OUTER JOIN vv ON xx.theId = vv.theId");
}
sub runTopSkimsQueriesNow() {
    my ($theLimit, $what, $site) = @_;

    print "# updating topPerf $what tables for NOW\n";

    my $idInPathTable    = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "SKIMS" ) {
        $idInPathTable    = "skimId";
        $destinationTable = "${site}_topPerfSkimsNow";
        $pastTable        = "${site}_topPerfSkimsPast";
    } elsif ( $what eq "TYPES" ) {
        $idInPathTable    = "typeId";
        $destinationTable = "${site}_topPerfTypesNow";
        $pastTable        = "${site}_topPerfTypesPast";
    } else {
        die "Invalid arg, expected SKIMS or TYPES\n";
    }

    # now jobs
    &runQuery("INSERT INTO jj
        SELECT $idInPathTable,
               COUNT(DISTINCT pId, clientHId ) AS n
        FROM   ${site}_openedSessions os, ${site}_openedFiles of, ${site}_paths p
        WHERE  os.id = of.sessionId
           AND of.pathId = p.id
        GROUP BY $idInPathTable");

    # now files
    &runQuery("REPLACE INTO ff 
        SELECT tmp.$idInPathTable,
               COUNT(tmp.pathId) AS n,
               SUM(tmp.size)/(1024*1024)  AS s
        FROM   ( SELECT DISTINCT $idInPathTable, pathId, size
                 FROM   ${site}_openedSessions os, ${site}_openedFiles of, ${site}_paths p
                 WHERE  os.id = of.sessionId
                    AND of.pathId = p.id
               ) AS tmp
        GROUP BY tmp.$idInPathTable");

    # now users
    &runQuery("REPLACE INTO uu 
        SELECT $idInPathTable,
               COUNT(DISTINCT userId) AS n
        FROM   ${site}_openedSessions os, ${site}_openedFiles of, ${site}_paths p
        WHERE  os.id = of.sessionId
           AND of.pathId = p.id
        GROUP BY $idInPathTable");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM $pastTable");
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM uu ORDER BY n DESC LIMIT $theLimit");

    &runQuery("TRUNCATE TABLE $destinationTable");

    ## and finally insert the new data
    &runQuery("INSERT INTO $destinationTable
        SELECT DISTINCT xx.theId,
               IFNULL(jj.n, 0) AS jobs,
               IFNULL(ff.n, 0) AS files, 
               IFNULL(ff.s, 0) AS fSize,
               IFNULL(uu.n, 0) AS users
        FROM   xx 
               LEFT OUTER JOIN jj ON xx.theId = jj.theId
               LEFT OUTER JOIN ff ON xx.theId = ff.theId
               LEFT OUTER JOIN uu ON xx.theId = uu.theId");
}

