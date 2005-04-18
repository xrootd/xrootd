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
if ( @ARGV ne 4 ) {
    print "Expected arg: <inputFileName> <dbName> <mySQLUser> ";
    print "<updateInterval>\n";
    exit;
}

my $inFName   = $ARGV[0];
my $dbName    = $ARGV[1];
my $mySQLUser = $ARGV[2];
my $updInt    = $ARGV[3];


$initFlag = 1;

# prepare counters for updating top performers tables
                      # update data for top perf 1 hour  period every  1 min
$dUpdatesFreq =   30; # update data for top perf 1 day   period every 30 min
$mUpdatesFreq =  720; # update data for top perf 1 month period every 12 hours
$yUpdatesFreq = 1440; # update data for top perf 1 year  period every 24 hours
$dCounter  = $dUpdatesFreq;
$mCounter  = $mUpdatesFreq;
$yCounter  = $yUpdatesFreq;

$loadFreq1stFail =   60;  # File size Loading frequency for files that have failed once,    every hour.
$loadFreq2ndFail =  720;  # File size Loading frequency for files that have failed twice,   every 12 hours.
$loadFreq3rdFail = 1440;  # File size Loading frequency for files that have failed 3 times, every 24 hour.


my $stopFName = "$inFName.stop";
my $noSleeps = $updInt/5;

# Load missing file size info
&loadFileSizes(-1000);
 
#start an infinite loop
while ( 1 ) {
    &doLoading();
    # sleep in 5 sec intervals, catch "stop" signal in between each sleep
    for ( $i=0 ; $i<$noSleeps ; $i++) {
        sleep(5);
        if ( -e $stopFName ) {
            unlink $stopFName;
            exit;
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

    # lock the file
    unless ( $lockF = &lockTheFile($inFName) ) {
	$dbh->disconnect();
	return;
    }
    # set the load time.
    $loadT = &timestamp();

     # open the input file for reading
    unless ( open $inF, "< $inFName" ) {
	print "Can't open file $inFName for reading\n";
	&unlockTheFile($lockF);
	$dbh->disconnect();
	return;
    }
    # read the file, load the data, close the file
    print "Loading...\n";
    $nr = 1;
    while ( $_ = <$inF> ) {
	#print "processing $_";
	chop;
	if ( $_ =~ m/^u/ ) { &loadOpenSession($_);  }
	if ( $_ =~ m/^d/ ) { &loadCloseSession($_); }
	if ( $_ =~ m/^o/ ) { &loadOpenFile($_);     }
	if ( $_ =~ m/^c/ ) { &loadCloseFile($_);    }
	if ( $nr % 10000 == 0 ) {
            $ts = &timestamp();
	    print "$ts $nr\n";
	}
	$nr += 1;
    }

    # $nMin, $nHour and $nDay start from 0
    &loadStatsLastHour();
    if ( $nMin % 60 == 59 ) {
        &loadStatsLastDay();
        if ( $nHour % 24 == 23 ) {
            &loadStatsLastMonth();
            $nDay += 1;
            if ( (localtime)[3] == 1 ) { &loadStatsAllMonths(); }
        }
        $nHour += 1;
    }
    $nMin += 1;  
                                                                                                                                       
    # load file sizes.
    if       ( ! ($nMin % $loadFreq3rdFail) ) {
         &loadFileSizes( -3 );               
    } elsif ( ! ($nMin % $loadFreq2ndFail) ) {
         &loadFileSizes( -2 );               
    } elsif ( ! ($nMin % $loadFreq1stFail) ) {
         &loadFileSizes( -1 );               
    } else {
         &loadFileSizes( 0 ); # every minute
    }       

    close $inF;
    # make a backup, remove the input file
    my $backupFName = "$inFName.backup";
    `touch $backupFName; cat $inFName >> $backupFName; rm $inFName`;
    # unlock the lock file
    unlockTheFile($lockF);

    &reloadTopPerfTables();

    # disconnect from db
    $dbh->disconnect();

    $ts = &timestamp();
    print "$ts All done, processed $nr entries.\n";

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
    my ($line) = @_;

    my ($u, $sessionId, $user, $pid, $clientHost, $srvHost) = split('\t', $line);
    #print "u=$u, id=$id, user=$user, pid=$pid, ch=$clientHost, sh=$srvHost\n";

    my $userId       = findOrInsertUserId($user);
    my $clientHostId = findOrInsertHostId($clientHost);
    my $serverHostId = findOrInsertHostId($srvHost);

    #print "uid=$userId, chid=$clientHostId, shd=$serverHostId\n";
    &runQuery("INSERT INTO rtOpenedSessions (id, userId, pId, clientHId, serverHId) VALUES ($sessionId, $userId, $pid, $clientHostId, $serverHostId)");
}


sub loadCloseSession() {
    my ($line) = @_;

    my ($d, $sessionId, $sec, $timestamp) = split('\t', $line);
    #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";




    # find if there is corresponding open session, if not don't bother
    my ($userId, $pId, $clientHId, $serverHId) = 
	&runQueryWithRet("SELECT userId, pId, clientHId, serverHId FROM rtOpenedSessions WHERE id = $sessionId");
    if ( ! $pId  ) {
	return;
    }
    #print "received decent data for sId $sessionId: uid=$userId, pid = $pId, cId=$clientHId, sId=$serverHId\n";

    # remove it from the open session table
    &runQuery("DELETE FROM rtOpenedSessions WHERE id = $sessionId;");

    # and insert into the closed
    &runQuery("INSERT INTO rtClosedSessions (id, userId, pId, clientHId, serverHId, duration, disconnectT) VALUES ($sessionId, $userId, $pId, $clientHId, $serverHId, $sec, \"$timestamp\");");
}


sub loadOpenFile() {
    my ($line) = @_;

    my ($o, $fileId, $user, $pid, $clientHost, $path, $openTime, $srvHost) = 
	split('\t', $line);
    #print "\no=$o, id=$id, user=$user, pid=$pid, ch=$clientHost, p=$path, ";
    #print "time=$openTime, srvh=$srvHost\n";

    my $sessionId = findSessionId($user, $pid, $clientHost, $srvHost);
    if ( ! $sessionId ) {
	#print "session id not found for $user $pid $clientHost $srvHost\n";
	return; # error: no corresponding session id
    }

    my $pathId = findOrInsertPathId($path);
    if ( ! $pathId ) {
	return; # error
    }

    &runQuery("INSERT INTO rtOpenedFiles (id, sessionId, pathId, openT) VALUES ($fileId, $sessionId, $pathId, \"$openTime\")");
}

sub loadCloseFile() {
    my ($line) = @_;

    my ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t', $line);
    #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

    # find if there is corresponding open file, if not don't bother
    my ($sessionId, $pathId, $openT) = 
	&runQueryWithRet("SELECT sessionId, pathId, openT FROM rtOpenedFiles WHERE id = $fileId");
    if ( ! $sessionId ) {
	return;
    }

    # remove it from the open files table
    &runQuery("DELETE FROM rtOpenedFiles WHERE id = $fileId;");

    # and insert into the closed
    &runQuery("INSERT INTO rtClosedFiles (id, sessionId, openT, closeT, pathId, bytesR, bytesW) VALUES ($fileId, $sessionId, \"$openT\", \"$closeT\", $pathId, $bytesR, $bytesW);");
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost) = @_;

    my $userId       = findOrInsertUserId($user);
    my $clientHostId = findOrInsertHostId($clientHost);
    my $serverHostId = findOrInsertHostId($srvHost);

    return &runQueryWithRet("SELECT id FROM rtOpenedSessions WHERE userId=$userId AND pId=$pid AND clientHId=$clientHostId AND serverHId=$serverHostId;");
}


sub findOrInsertUserId() {
    my ($userName) = @_;

    my $userId = $userIds{$userName};
    if ( $userId ) {
        return $userId;
    }
    $userId = &runQueryWithRet("SELECT id FROM users WHERE name = \"$userName\"");
    if ( $userId ) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO users (name) VALUES (\"$userName\");");

	$userId = &runQueryWithRet("SELECT LAST_INSERT_ID();");
    }
    $userIds{$userName} = $userId;
    return $userId;
}

sub findOrInsertHostId() {
    my ($hostName) = @_;

    my $hostId = $hostIds{$hostName};
    if ( $hostId ) {
        return $hostId;
    }
    $hostId = &runQueryWithRet("SELECT id FROM hosts WHERE hostName = \"$hostName\"");
    if ( $hostId ) {
	#print "Will reuse hostId $clientHostId for $hostName\n";
    } else {
	#print "$hostName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO hosts (hostName) VALUES (\"$hostName\");");

	$hostId = &runQueryWithRet("SELECT LAST_INSERT_ID();");
    }
    $hostIds{$hostName} = $hostId;
    return $hostId;
}

sub findOrInsertPathId() {
    use vars qw($pathId $typeId $skimId);
    my ($path) = @_;
                                                                                                      
    $pathId = $pathIds{$path};
    if ( $pathId ) {
        #print "from cache: $pathId for $path\n";
        return $pathId;
    }
    ($pathId, $typeId, $skimId) =
        &runQueryWithRet("SELECT id, typeId, skimId FROM paths WHERE name = \"$path\"");

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
            $typeId = &runQueryWithRet("SELECT LAST_INSERT_ID();");
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
                $skimId = &runQueryWithRet("SELECT LAST_INSERT_ID();");
            }
        }
        &runQuery("INSERT INTO paths (name, typeId, skimId, size) VALUES (\"$path\", $typeId, $skimId, 0 );");
        $pathId = &runQueryWithRet("SELECT LAST_INSERT_ID();");
                                                                                                      
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
    #print "$sql;\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
    return $sth->fetchrow_array;
}

sub runQuery() {
    my ($sql) = @_;
    #print "$sql;\n";
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
                                 FROM statsLastHour 
                                WHERE date = \"$lastTime\"");
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

    ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastMonth");
    if ( $lastTime ) {
        $nDay = &runQueryWithRet("SELECT seqNo FROM statsLastMonth WHERE date = \"$lastTime\"");
        $nDay += 1;
    } else {
        $nDay = 0;
    }
    $bbkListSize = 250;
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

sub loadFileSizes() {
    use vars qw($sizeIndex $fromId $toId $path $size @files @inBbk);
    ($sizeIndex) = @_;
    &runQuery("CREATE TEMPORARY TABLE zerosize  (theId INT AUTO_INCREMENT, name VARCHAR(256), INDEX (theId))"); 
    &runQuery("INSERT INTO zerosize SELECT name FROM paths WHERE size BETWEEN $sizeIndex AND 0");
     
    $fromId = 1;
    $toId = $bbkListSize;
    while () {
       @files = &runQueryWithRet("SELECT name FROM zerosize WHERE theId BETWEEN $fromId AND $toId"); 
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
           &runQuery("UPDATE files SET size = $size WHERE name = $path");
       }
       # decrement size by 1 for files that failed bbk.
       foreach $path ( @files ) {
           if ( ! grep { $_ eq $path } @inBbk ) {
               &runQuery("UPDATE files SET size = size - 1 WHERE name = $path");
           }
       }

       last if ( @files < $bbkListSize );
       $fromId += $bbkListSize;
       $toId += $bbkListSize;
    }
    &runQuery("DROP TABLE IF EXISTS zerosize");
}


sub loadStatsLastHour() {
    use vars qw($seqNo $noJobs $noUsers $noUniqueF $noNonUniqueF $deltaJobs $jobs_p 
                $deltaUsers $users_p $deltaUniqueF $uniqueF_p $deltaNonUniqueF $nonUniqueF_p);
    $seqNo = $nMin % 60;
    &runQuery("DELETE FROM statsLastHour WHERE seqNo = $seqNo");
    ($noJobs, $noUsers) = &runQueryWithRet("SELECT COUNT(DISTINCT CONCAT(pId, clientHId)), COUNT(DISTINCT userId) 
                                                FROM rtOpenedSessions");

    ($noUniqueF, $noNonUniqueF) = &runQueryWithRet("SELECT COUNT(DISTINCT pathId), COUNT(*) 
                                                        FROM rtOpenedFiles");
    &runQuery("INSERT INTO statsLastHour 
                          (seqNo, date, noJobs, noUsers, noUniqueF, noNonUniqueF) 
                   VALUES ($seqNo, \"$loadT\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF)");

    &runQuery("DELETE FROM rtChanges");
    $deltaJobs = $noJobs - $lastNoJobs; 
    $jobs_p = $lastNoJobs > 0 ? &roundoff( 100 * $deltaJobs / $lastNoJobs ) : -1;
    $deltaUsers = $noUsers - $lastNoUsers;
    $users_p = $lastNoUsers > 0 ? &roundoff( 100 * $deltaUsers / $lastNoUsers ) : -1;
    $deltaUniqueF = $noUniqueF - $lastNoUniqueF;
    $uniqueF_p = $lastNoUniqueF > 0 ? &roundoff( 100 * $deltaUniqueF / $lastNoUniqueF ) : -1;
    $deltaNonUniqueF = $noNonUniqueF - $lastNoNonUniqueF;
    $nonUniqueF_p = $lastNoNonUniqueF > 0 ? &roundoff( 100 * $deltaNonUniqueF / $lastNoNonUniqueF ) : -1;
    &runQuery("INSERT INTO rtChanges 
                          (jobs, jobs_p, users, users_p, uniqueF, uniqueF_p, 
                           nonUniqueF, nonUniqueF_p, lastUpdate)
                   VALUES ($deltaJobs, $jobs_p, $deltaUsers, $users_p, $deltaUniqueF, $uniqueF_p, 
                           $deltaNonUniqueF, $nonUniqueF_p, \"$loadT\")");
    $lastNoJobs = $noJobs;
    $lastNoUsers = $noUsers;
    $lastNoUniqueF = $noUniqueF;
    $lastNoNonUniqueF = $noNonUniqueF;
}

sub loadStatsLastDay() {
    my $seqNo = $nHour % 24;
    &runQuery("DELETE FROM statsLastDay WHERE seqNo = $seqNo");

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastHour");

    &runQuery("INSERT INTO statsLastDay 
                          (seqNo, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                   VALUES ($seqNo, \"$loadT\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
                           $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                           $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");

}

sub loadStatsLastMonth() {
    my $seqNo = $nDay % 31;
    &runQuery("DELETE FROM statsLastMonth WHERE seqNo = $seqNo");

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastDay");

    &runQuery("INSERT INTO statsLastMonth 
                          (seqNo, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                    VALUES ($seqNo, \"$loadT\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
                            $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                            $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");

}

sub loadStatsAllMonths() {
    # note that (localtime)[4] returns month in the range 0 - 11 and normally should be
    # increased by 1 to show the current month.
    $lastMonth = (localtime)[4];

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastMonth WHERE MNTH(date) = \"$lastMonth\""); 

    &runQuery("INSERT INTO statsAllMonths 
                          (date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                    VALUES (\"$loadT\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
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
    # create tables
    &runQuery("CREATE TEMPORARY TABLE jj  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE ff  (theId INT, n INT, s INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE uu  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE vv  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE xx  (theId INT UNIQUE KEY, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE tmp (theId INT, n INT, s INT, INDEX (theId))");
    @tables = ("jj", "ff", "uu", "vv", "xx", "tmp");

    # run queries for past (last hour/day/month/year
    &runQueries4AllTopPerfTablesPast("1 HOUR", "hour", 20);
    if ( $dCounter == $dUpdatesFreq ) {
	&runQueries4AllTopPerfTablesPast("1 DAY", "day", 20);
	$dCounter = 0;
    } else {
	$dCounter += 1;
    }
    if ( $mCounter == $mUpdatesFreq ) {
	&runQueries4AllTopPerfTablesPast("1 MONTH", "month", 20);
	$mCounter = 0;
    } else {
	$mCounter += 1;
    }
    if ( $yCounter == $yUpdatesFreq ) {
	&runQueries4AllTopPerfTablesPast("1 YEAR", "year", 20);
	$yCounter = 0;
    } else {
	$yCounter += 1;
    }
    # run queries for now
    &runQueries4AllTopPerfTablesNow(20);

    # delete tables
    foreach $table (@tables) { &runQuery("DROP TABLE IF EXISTS $table"); }
}

sub runQueries4AllTopPerfTablesPast() {
    my ($theInterval, $theKeyword, $theLimit) = @_;

    &runTopUsrFsQueriesPast($theInterval, $theKeyword, $theLimit, "USERS");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
    &runTopSkimsQueriesPast($theInterval, $theKeyword, $theLimit, "SKIMS");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
    &runTopSkimsQueriesPast($theInterval, $theKeyword, $theLimit, "TYPES");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
    &runTopUsrFsQueriesPast($theInterval, $theKeyword, $theLimit, "FILES");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
}

sub runQueries4AllTopPerfTablesNow() {
    my ($theLimit) = @_;

    &runTopUsrFsQueriesNow($theLimit, "USERS");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
    &runTopSkimsQueriesNow($theLimit, "SKIMS");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
    &runTopSkimsQueriesNow($theLimit, "TYPES");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
    &runTopUsrFsQueriesNow($theLimit, "FILES");
    foreach $table (@tables) { &runQuery("DELETE FROM $table"); }
}

sub runTopUsrFsQueriesPast() {
    my ($theInterval, $theKeyword, $theLimit, $what) = @_;

    print "# updating topPerf $what tables for $theInterval\n";

    my $idInTable        = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "USERS" ) {
        $idInTable        = "userId";
        $destinationTable = "topPerfUsersPast";
    } elsif ( $what eq "FILES" ) {
        $idInTable        = "pathId";
        $destinationTable = "topPerfFilesPast";
    } else {
        die "Invalid arg, expected USERS or FILES\n";
    }

    # past jobs
    if ( $what eq "USERS" ) {
        &runQuery("INSERT INTO jj
            SELECT $idInTable, 
                   COUNT(DISTINCT CONCAT(pId, clientHId)) AS n
            FROM   rtClosedSessions
            WHERE  disconnectT > DATE_SUB(NOW(), INTERVAL $theInterval)
                   GROUP BY $idInTable");
    } elsif ( $what eq "FILES" ) {
        &runQuery("INSERT INTO jj
            SELECT $idInTable, 
                   COUNT(DISTINCT CONCAT(pId, clientHId)) AS n
            FROM   rtClosedSessions cs, rtClosedFiles cf
            WHERE  cs.id = cf.sessionId AND
                   disconnectT > DATE_SUB(NOW(), INTERVAL $theInterval)
                   GROUP BY $idInTable");
    }
    # past files - through opened sessions
    &runQuery("INSERT INTO tmp
       SELECT $idInTable, 
              COUNT(DISTINCT pathId) AS n,
              SUM(size)/(1024*1024) AS s
       FROM   rtOpenedSessions os, rtClosedFiles cf, paths 
       WHERE  os.id = cf.sessionId AND
              cf.pathId = paths.id AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY $idInTable");
    # past files - through closed sessions
    &runQuery("INSERT INTO tmp
       SELECT $idInTable, 
              COUNT(DISTINCT pathId) AS n,
              SUM(size)/(1024*1024) AS s
       FROM   rtClosedSessions cs, rtClosedFiles cf, paths
       WHERE  cs.id = cf.sessionId AND
              cf.pathId = paths.id AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY $idInTable");
    # past files - merge results
    &runQuery("INSERT INTO ff 
        SELECT DISTINCT theId, SUM(n), SUM(s) FROM tmp GROUP BY theId");
    # cleanup tmp table
    &runQuery("DELETE FROM tmp");
    # past volume - through opened sessions
    &runQuery("INSERT INTO tmp (theId, n)
        SELECT $idInTable, SUM(bytesR)/(1024*1024) AS n
        FROM   rtOpenedSessions os, rtClosedFiles cf
        WHERE  os.id = cf.sessionId AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY $idInTable");
    # past volume - through closed sessions
    &runQuery("INSERT INTO tmp (theId, n)
        SELECT $idInTable, SUM(bytesR)/(1024*1024) AS n
        FROM   rtClosedSessions cs, rtClosedFiles cf
        WHERE  cs.id = cf.sessionId AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY $idInTable");
    # past volume - merge results
    &runQuery("INSERT INTO vv SELECT DISTINCT theId, SUM(n) FROM tmp GROUP BY theId");
    # cleanup tmp table
    &runQuery("DELETE FROM tmp");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM vv ORDER BY n DESC LIMIT $theLimit");

    ## delete old data
    &runQuery("DELETE FROM $destinationTable WHERE timePeriod LIKE \"$theKeyword\"");

    ## and finally insert the new data
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
}


sub runTopUsrFsQueriesNow() {
    my ($theLimit, $what) = @_;

    print "# updating topPerf $what tables for NOW\n";

    my $idInTable        = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "USERS" ) {
        $idInTable        = "userId";
        $destinationTable = "topPerfUsersNow";
        $pastTable        = "topPerfUsersPast";
    } elsif ( $what eq "FILES" ) {
        $idInTable        = "pathId";
        $destinationTable = "topPerfFilesNow";
        $pastTable        = "topPerfFilesPast";
    } else {
        die "Invalid arg, expected USERS or FILES\n";
    }

    # now jobs
    &runQuery("INSERT INTO jj
        SELECT $idInTable, COUNT(DISTINCT CONCAT(pId, clientHId) ) AS n
        FROM   rtOpenedSessions os, rtOpenedFiles of
        WHERE  os.id = of.sessionId
               GROUP BY $idInTable");
    # now files
    &runQuery ("INSERT INTO ff 
        SELECT $idInTable, 
               COUNT(DISTINCT pathId) AS n,
               SUM(size)/(1024*1024) AS s
        FROM   rtOpenedSessions os, rtOpenedFiles of, paths
        WHERE  os.id = of.sessionId AND
               of.pathId = paths.id
               GROUP BY $idInTable");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM $pastTable;");
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");

    ## and finally insert the new data
    &runQuery("INSERT INTO $destinationTable
        SELECT DISTINCT xx.theId,
               IFNULL(jj.n, 0) AS jobs,
               IFNULL(ff.n, 0) AS files, 
               IFNULL(ff.s, 0) AS fSize
        FROM   xx 
               LEFT OUTER JOIN jj ON xx.theId = jj.theId
               LEFT OUTER JOIN ff ON xx.theId = ff.theId");
}

  
sub runTopSkimsQueriesPast() {
    my ($theInterval, $theKeyword, $theLimit, $what) = @_;

    print "# updating topPerf $what tables for $theInterval\n";

    my $idInPathTable    = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "SKIMS" ) {
        $idInPathTable    = "skimId";
        $destinationTable = "topPerfSkimsPast";
    } elsif ( $what eq "TYPES" ) {
        $idInPathTable    = "typeId";
        $destinationTable = "topPerfTypesPast";
    } else {
        die "Invalid arg, expected SKIMS or TYPES\n";
    }

    # past jobs
    &runQuery("REPLACE INTO jj
        SELECT $idInPathTable, 
               COUNT(DISTINCT CONCAT(pId, clientHId) ) AS n
        FROM   rtClosedSessions cs, rtClosedFiles cf, paths
        WHERE  cs.id = cf.sessionId AND
               cf.pathId = paths.id AND
               disconnectT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY $idInPathTable");

    # past files - through opened sessions
    &runQuery("INSERT INTO tmp
        SELECT $idInPathTable,
               COUNT(DISTINCT pathId) AS n,
               SUM(size)/(1024*1024)  AS s
        FROM   rtOpenedSessions os, rtClosedFiles cf, paths
        WHERE  os.id = cf.sessionId AND
               cf.pathId = paths.id AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY $idInPathTable");
    # past files - through closed sessions
    &runQuery("INSERT INTO tmp
        SELECT $idInPathTable,
               COUNT(DISTINCT pathId) AS n,
               SUM(size)/(1024*1024)  AS s
        FROM   rtClosedSessions cs, rtClosedFiles cf, paths
        WHERE  cs.id = cf.sessionId AND
               cf.pathId = paths.id AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY $idInPathTable");
    # past files - merge result
    &runQuery("INSERT INTO ff
         SELECT DISTINCT theId, SUM(n), SUM(s) FROM tmp GROUP BY theId");
    # cleanup temporary table
    &runQuery("DELETE FROM tmp");
    # past users
    &runQuery("REPLACE INTO uu
        SELECT $idInPathTable, 
               COUNT(DISTINCT userId) AS n
        FROM   rtClosedSessions cs, rtClosedFiles cf, paths
        WHERE  cs.id = cf.sessionId AND
               cf.pathId = paths.id AND
               disconnectT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY $idInPathTable");
    # past volume - through opened sessions
    &runQuery("INSERT INTO tmp (theId, n)
         SELECT $idInPathTable, SUM(bytesR)/(1024*1024) AS n
         FROM   rtOpenedSessions os, rtClosedFiles cf, paths
         WHERE  os.id = cf.sessionId AND
                cf.pathId = paths.id AND
                closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
                GROUP BY $idInPathTable");
    # past volume - through closed sessions
    &runQuery("INSERT INTO tmp (theId, n)
         SELECT $idInPathTable, SUM(bytesR)/(1024*1024) AS n
         FROM   rtClosedSessions os, rtClosedFiles cf, paths
         WHERE  os.id = cf.sessionId AND
                cf.pathId = paths.id AND
                closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
                GROUP BY $idInPathTable");
    # past volume - merge result
    &runQuery("INSERT INTO vv SELECT DISTINCT theId, SUM(n) FROM tmp GROUP BY theId");
    # cleanup temporary table
    &runQuery("DELETE FROM tmp");


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
    my ($theLimit, $what) = @_;

    print "# updating topPerf $what tables for NOW\n";

    my $idInPathTable    = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "SKIMS" ) {
        $idInPathTable    = "skimId";
        $destinationTable = "topPerfSkimsNow";
        $pastTable        = "topPerfSkimsPast";
    } elsif ( $what eq "TYPES" ) {
        $idInPathTable    = "typeId";
        $destinationTable = "topPerfTypesNow";
        $pastTable        = "topPerfTypesPast";
    } else {
        die "Invalid arg, expected SKIMS or TYPES\n";
    }

    # now jobs
    &runQuery("INSERT INTO jj
        SELECT $idInPathTable,
               COUNT(DISTINCT CONCAT(pId, clientHId) ) AS n
        FROM   rtOpenedSessions os, rtOpenedFiles of, paths
        WHERE  os.id = of.sessionId AND
               of.pathId = paths.id
               GROUP BY $idInPathTable");

    # now files
    &runQuery("REPLACE INTO ff 
        SELECT $idInPathTable,
               COUNT(DISTINCT pathId) AS n,
               SUM(size)/(1024*1024)  AS s
        FROM   rtOpenedSessions os, rtOpenedFiles of, paths
        WHERE  os.id = of.sessionId AND
               of.pathId = paths.id
               GROUP BY $idInPathTable");

    # now users
    &runQuery("REPLACE INTO uu 
        SELECT $idInPathTable,
               COUNT(DISTINCT userId) AS n
        FROM   rtOpenedSessions os, rtOpenedFiles of, paths
        WHERE  os.id = of.sessionId AND
               of.pathId = paths.id
               GROUP BY $idInPathTable");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM $pastTable;");
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM uu ORDER BY n DESC LIMIT $theLimit");

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

