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


my $stopFName = "$inFName.stop";
my $noSleeps = $updInt/5;

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
    $userId = &runQueryWithRet("SELECT id FROM users WHERE userName = \"$userName\"");
    if ( $userId ) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO users (userName) VALUES (\"$userName\");");

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
        &runQueryWithRet("SELECT id, typeId, skimId FROM paths WHERE path = \"$path\"");

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
        &runQuery("INSERT INTO paths (path, typeId, skimId) VALUES (\"$path\", $typeId, $skimId);");
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
    #print "$sql\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
    return $sth->fetchrow_array;
}

sub runQuery() {
    my ($sql) = @_;
    #print "$sql\n";
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
                           FROM statsLastHour");

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
    &runQueries4AllTopPerfTables("1 HOUR", "hour", 20);
    if ( $dCounter == $dUpdatesFreq ) {
	&runQueries4AllTopPerfTables("1 DAY", "day", 20);
	$dCounter = 0;
    } else {
	$dCounter += 1;
    }
    if ( $mCounter == $mUpdatesFreq ) {
	&runQueries4AllTopPerfTables("1 MONTH", "month", 20);
	$mCounter = 0;
    } else {
	$mCounter += 1;
    }
    if ( $yCounter == $yUpdatesFreq ) {
	&runQueries4AllTopPerfTables("1 YEAR", "year", 20);
	$yCounter = 0;
    } else {
	$yCounter += 1;
    }
}

sub runQueries4AllTopPerfTables() {
    my ($theInterval, $theKeyword, $theLimit) = @_;
    &runTopUsersQueries($theInterval, $theKeyword, $theLimit);
    &runTopSkimsQueries($theInterval, $theKeyword, $theLimit);
   #&runTopFilesQueries($theInterval, $theKeyword, $theLimit);
}

# nj - now jobs
# nf - now files
# pj - past jobs
# pf - past files
# vf - past read volume
sub runTopUsersQueries() {
    my ($theInterval, $theKeyword, $theLimit) = @_;

    print "updating topPerf USERS tables for $theInterval\n";

    my $sql_prepTable_nj   = "CREATE TEMPORARY TABLE nj  (uId INT, n INT)";    
    my $sql_prepTable_nf   = "CREATE TEMPORARY TABLE nf  (uId INT, n INT)";
    my $sql_prepTable_pj   = "CREATE TEMPORARY TABLE pj  (uId INT, n INT)";
    my $sql_prepTable_pf   = "CREATE TEMPORARY TABLE pf  (uId INT, n INT)";
    my $sql_prepTable_pv   = "CREATE TEMPORARY TABLE pv  (uId INT, n INT)";
    my $sql_prepTable_tmp  = "CREATE TEMPORARY TABLE tmp (uId INT, n INT)";
    my $sql_prepTable_xx   = "CREATE TEMPORARY TABLE xx  (uId INT UNIQUE KEY)";

    my $sql_cleanTable_tmp = "DELETE FROM tmp;";

    my $sql_dropTable_nj   = "DROP TABLE IF EXISTS nj";
    my $sql_dropTable_nf   = "DROP TABLE IF EXISTS nf";
    my $sql_dropTable_pj   = "DROP TABLE IF EXISTS pj";
    my $sql_dropTable_pf   = "DROP TABLE IF EXISTS pf";
    my $sql_dropTable_pv   = "DROP TABLE IF EXISTS pv";
    my $sql_dropTable_tmp  = "DROP TABLE IF EXISTS tmp";
    my $sql_dropTable_xx   = "DROP TABLE IF EXISTS xx";

    # now jobs
    my $sql_find_nj = "INSERT INTO nj
        SELECT userId, COUNT(DISTINCT CONCAT(pId, clientHId) ) AS n
        FROM   rtOpenedSessions os, rtOpenedFiles of
        WHERE  os.id = of.sessionId
               GROUP BY userId";

    # now files
    my $sql_find_nf = "INSERT INTO nf 
        SELECT userId, COUNT(DISTINCT pathId) AS n
        FROM   rtOpenedSessions os, rtOpenedFiles of
        WHERE  os.id = of.sessionId
               GROUP BY userId";

    # past jobs
    my $sql_find_pj = "INSERT INTO pj
        SELECT userId, 
               COUNT(DISTINCT CONCAT(pId, clientHId)) AS n
        FROM   rtClosedSessions os
        WHERE  disconnectT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY userId";

    # past files
        # through opened sessions
    my $sql_find_pf_os = "INSERT INTO tmp
        SELECT userId, COUNT(DISTINCT pathId) AS n
        FROM   rtOpenedSessions os, rtClosedFiles cf
        WHERE  os.id = cf.sessionId AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY userId";
        # through closed sessions
    my $sql_find_pf_cs = "INSERT INTO tmp
        SELECT userId, COUNT(DISTINCT pathId) AS n
        FROM   rtClosedSessions os, rtClosedFiles cf
        WHERE  os.id = cf.sessionId AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY userId";
        # merge result
    my $sql_merge_pf = "INSERT INTO pf
        SELECT uId, SUM(n) 
        FROM   tmp
        GROUP BY uId";

    # past volume 
        # through opened sessions
    my $sql_find_pv_os = "INSERT INTO tmp
        SELECT userId, SUM(bytesR)/(1024*1024) AS n
        FROM   rtOpenedSessions os, rtClosedFiles cf
        WHERE  os.id = cf.sessionId AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY userId";
        # through closed sessions
    my $sql_find_pv_cs = "INSERT INTO tmp
        SELECT userId, SUM(bytesR)/(1024*1024) AS n
        FROM   rtClosedSessions os, rtClosedFiles cf
        WHERE  os.id = cf.sessionId AND
               closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
               GROUP BY userId";
        # merge result
    my $sql_merge_pv = "INSERT INTO pv
        SELECT uId, SUM(n) 
        FROM   tmp
        GROUP BY uId";

    ##### now find all names for top X for each sorting 
    my $sql_insert_nj = "REPLACE INTO xx SELECT uId FROM nj ORDER BY n DESC LIMIT $theLimit";
    my $sql_insert_nf = "REPLACE INTO xx SELECT uId FROM nf ORDER BY n DESC LIMIT $theLimit";
    my $sql_insert_pj = "REPLACE INTO xx SELECT uId FROM pj ORDER BY n DESC LIMIT $theLimit";
    my $sql_insert_pf = "REPLACE INTO xx SELECT uId FROM pf ORDER BY n DESC LIMIT $theLimit";
    my $sql_insert_pv = "REPLACE INTO xx SELECT uId FROM pv ORDER BY n DESC LIMIT $theLimit";

    ## delete old data
    my $sql_deleteOldData = 
        "DELETE FROM topPerfUsers  WHERE timePeriod LIKE \"$theKeyword\"";

    ## and finally insert the new data
    my $sql_insert = "INSERT INTO topPerfUsers
        SELECT userName, 
               IFNULL(nj.n, 0) AS nJobs,
               IFNULL(nf.n, 0) AS nFiles, 
               IFNULL(pj.n, 0) AS pJobs, 
               IFNULL(pf.n, 0) AS pFiles, 
               IFNULL(pv.n, 0) AS pVol, 
               \"$theKeyword\"
        FROM   users, xx 
               LEFT OUTER JOIN nj ON xx.uId = nj.uId
               LEFT OUTER JOIN nf ON xx.uId = nf.uId
               LEFT OUTER JOIN pj ON xx.uId = pj.uId
               LEFT OUTER JOIN pf ON xx.uId = pf.uId
               LEFT OUTER JOIN pv ON xx.uId = pv.uId
        WHERE  xx.uId = users.id";

    ######## prepare ########
    my $sth_prepTable_nj  = $dbh->prepare($sql_prepTable_nj )  or die "\"$sql_prepTable_nj\",   $DBI::errstr\n";
    my $sth_prepTable_nf  = $dbh->prepare($sql_prepTable_nf )  or die "\"$sql_prepTable_nf\",   $DBI::errstr\n";
    my $sth_prepTable_pj  = $dbh->prepare($sql_prepTable_pj )  or die "\"$sql_prepTable_pj\",   $DBI::errstr\n";
    my $sth_prepTable_pf  = $dbh->prepare($sql_prepTable_pf )  or die "\"$sql_prepTable_pf\",   $DBI::errstr\n";
    my $sth_prepTable_pv  = $dbh->prepare($sql_prepTable_pv )  or die "\"$sql_prepTable_pv\",   $DBI::errstr\n";
    my $sth_prepTable_tmp = $dbh->prepare($sql_prepTable_tmp)  or die "\"$sql_prepTable_tmp\",  $DBI::errstr\n";
    my $sth_prepTable_xx  = $dbh->prepare($sql_prepTable_xx )  or die "\"$sql_prepTable_xx\",   $DBI::errstr\n";

    my $sth_cleanTable_tmp= $dbh->prepare($sql_cleanTable_tmp) or die "\"$sql_cleanTable_tmp\", $DBI::errstr\n";

    my $sth_dropTable_nj  = $dbh->prepare($sql_dropTable_nj )  or die "\"$sql_dropTable_nj\",   $DBI::errstr\n";
    my $sth_dropTable_nf  = $dbh->prepare($sql_dropTable_nf )  or die "\"$sql_dropTable_nf\",   $DBI::errstr\n";
    my $sth_dropTable_pj  = $dbh->prepare($sql_dropTable_pj )  or die "\"$sql_dropTable_pj\",   $DBI::errstr\n";
    my $sth_dropTable_pf  = $dbh->prepare($sql_dropTable_pf )  or die "\"$sql_dropTable_pf\",   $DBI::errstr\n";
    my $sth_dropTable_pv  = $dbh->prepare($sql_dropTable_pv )  or die "\"$sql_dropTable_pv\",   $DBI::errstr\n";
    my $sth_dropTable_tmp = $dbh->prepare($sql_dropTable_tmp)  or die "\"$sql_dropTable_tmp\",  $DBI::errstr\n";
    my $sth_dropTable_xx  = $dbh->prepare($sql_dropTable_xx )  or die "\"$sql_dropTable_xx\",   $DBI::errstr\n";

    my $sth_find_nj    = $dbh->prepare($sql_find_nj   ) or die "\"$sql_find_nj\",    $DBI::errstr\n";
    my $sth_find_nf    = $dbh->prepare($sql_find_nf   ) or die "\"$sql_find_nf\",    $DBI::errstr\n";
    my $sth_find_pj    = $dbh->prepare($sql_find_pj   ) or die "\"$sql_find_pj\",    $DBI::errstr\n";
    my $sth_find_pf_os = $dbh->prepare($sql_find_pf_os) or die "\"$sql_find_pf_os\", $DBI::errstr\n";
    my $sth_find_pf_cs = $dbh->prepare($sql_find_pf_cs) or die "\"$sql_find_pf_cs\", $DBI::errstr\n";
    my $sth_merge_pf   = $dbh->prepare($sql_merge_pf  ) or die "\"$sql_merge_pf\",   $DBI::errstr\n";
    my $sth_find_pv_os = $dbh->prepare($sql_find_pv_os) or die "\"$sql_find_pv_os\", $DBI::errstr\n";
    my $sth_find_pv_cs = $dbh->prepare($sql_find_pv_cs) or die "\"$sql_find_pv_cs\", $DBI::errstr\n";
    my $sth_merge_pv   = $dbh->prepare($sql_merge_pv  ) or die "\"$sql_merge_pv\",   $DBI::errstr\n";

    my $sth_insert_nj  = $dbh->prepare($sql_insert_nj ) or die "\"$sql_insert_nj\",  $DBI::errstr\n";
    my $sth_insert_nf  = $dbh->prepare($sql_insert_nf ) or die "\"$sql_insert_nf\",  $DBI::errstr\n";
    my $sth_insert_pj  = $dbh->prepare($sql_insert_pj ) or die "\"$sql_insert_pj\",  $DBI::errstr\n";
    my $sth_insert_pf  = $dbh->prepare($sql_insert_pf ) or die "\"$sql_insert_pf\",  $DBI::errstr\n";
    my $sth_insert_pv  = $dbh->prepare($sql_insert_pv ) or die "\"$sql_insert_pv\",  $DBI::errstr\n";

    my $sth_deleteOldData = $dbh->prepare($sql_deleteOldData) or die "\"$sql_deleteOldData\", $DBI::errstr\n";
    my $sth_insert        = $dbh->prepare($sql_insert)        or die "\"$sql_insert\",        $DBI::errstr\n";

    ######## execute ########
    $sth_prepTable_nj->execute()  or die "Failed to exec \"$sql_prepTable_nj\",  $DBI::errstr";
    $sth_prepTable_nf->execute()  or die "Failed to exec \"$sql_prepTable_nf\",  $DBI::errstr";
    $sth_prepTable_pj->execute()  or die "Failed to exec \"$sql_prepTable_pj\",  $DBI::errstr";
    $sth_prepTable_pf->execute()  or die "Failed to exec \"$sql_prepTable_pf\",  $DBI::errstr";
    $sth_prepTable_pv->execute()  or die "Failed to exec \"$sql_prepTable_pv\",  $DBI::errstr";
    $sth_prepTable_tmp->execute() or die "Failed to exec \"$sql_prepTable_tmp\", $DBI::errstr";
    $sth_prepTable_xx->execute()  or die "Failed to exec \"$sql_prepTable_xx\",  $DBI::errstr";


    $sth_find_nj->execute()        or die "Failed to exec \"$sql_find_nj\",        $DBI::errstr";
    $sth_find_nf->execute()        or die "Failed to exec \"$sql_find_nf\",        $DBI::errstr";
    $sth_find_pj->execute()        or die "Failed to exec \"$sql_find_pj\",        $DBI::errstr";
    $sth_find_pf_os->execute()     or die "Failed to exec \"$sql_find_pf_os\",     $DBI::errstr";
    $sth_find_pf_cs->execute()     or die "Failed to exec \"$sql_find_pf_cs\",     $DBI::errstr";
    $sth_merge_pf->execute()       or die "Failed to exec \"$sql_merge_pf\",       $DBI::errstr";
    $sth_cleanTable_tmp->execute() or die "Failed to exec \"$sql_cleanTable_tmp\", $DBI::errstr";
    $sth_find_pv_os->execute()     or die "Failed to exec \"$sql_find_pv_os\",     $DBI::errstr";
    $sth_find_pv_cs->execute()     or die "Failed to exec \"$sql_find_pv_cs\",     $DBI::errstr";
    $sth_merge_pf->execute()       or die "Failed to exec \"$sql_merge_pf\",       $DBI::errstr";
    $sth_cleanTable_tmp->execute() or die "Failed to exec \"$sql_cleanTable_tmp\", $DBI::errstr";

    $sth_insert_nj->execute() or die "Failed to exec \"$sql_insert_nj\", $DBI::errstr";
    $sth_insert_nf->execute() or die "Failed to exec \"$sql_insert_nf\", $DBI::errstr";
    $sth_insert_pj->execute() or die "Failed to exec \"$sql_insert_pj\", $DBI::errstr";
    $sth_insert_pf->execute() or die "Failed to exec \"$sql_insert_pf\", $DBI::errstr";
    $sth_insert_pv->execute() or die "Failed to exec \"$sql_insert_pv\", $DBI::errstr";

    $sth_deleteOldData->execute() or die "Failed to exec \"$sql_deleteOldData\", $DBI::errstr";
    $sth_insert->execute()        or die "Failed to exec \"$sql_insert\", $DBI::errstr";

    $sth_dropTable_nj->execute()  or die "Failed to exec \"$sql_dropTable_nj\",  $DBI::errstr";
    $sth_dropTable_nf->execute()  or die "Failed to exec \"$sql_dropTable_nf\",  $DBI::errstr";
    $sth_dropTable_pj->execute()  or die "Failed to exec \"$sql_dropTable_pj\",  $DBI::errstr";
    $sth_dropTable_pf->execute()  or die "Failed to exec \"$sql_dropTable_pf\",  $DBI::errstr";
    $sth_dropTable_pv->execute()  or die "Failed to exec \"$sql_dropTable_pv\",  $DBI::errstr";
    $sth_dropTable_tmp->execute() or die "Failed to exec \"$sql_dropTable_tmp\", $DBI::errstr";
    $sth_dropTable_xx->execute()  or die "Failed to exec \"$sql_dropTable_xx\",  $DBI::errstr";
}
  
sub runTopSkimsQueries() {
    my ($theInterval, $theKeyword, $theLimit) = @_;

    print "updating topPerf SKIMS tables for $theInterval\n";

}
