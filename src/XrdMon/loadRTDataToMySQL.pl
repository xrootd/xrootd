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


#start an infinite loop
while ( 1 ) {
    &doLoading();
    sleep($updInt);
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
    ($noJobs, $noUsers) = &runQueryWithRet("SELECT COUNT(*), COUNT(DISTINCT userId) 
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

    &runTopUsersQueries("1 HOUR", "hour", 20);
    if ( $dCounter == $dUpdatesFreq ) {
	&runTopUsersQueries("1 DAY", "day", 20);
	$dCounter = 0;
    } else {
	$dCounter += 1;
    }
    if ( $mCounter == $mUpdatesFreq ) {
	&runTopUsersQueries("1 MONTH", "month", 20);
	$mCounter = 0;
    } else {
	$mCounter += 1;
    }
    if ( $yCounter == $yUpdatesFreq ) {
	&runTopUsersQueries("1 YEAR", "year", 20);
	$yCounter = 0;
    } else {
	$yCounter += 1;
    }
}


sub runTopUsersQueries() {
    my ($theInterval, $theKeyword, $theLimit) = @_;

    print "updating topPerf tables for $theInterval\n";

    my $sql_prepTable_tt       = "CREATE TEMPORARY TABLE tt (userId INT PRIMARY KEY)";
    my $sql_prepTable_t_       = "CREATE TEMPORARY TABLE t_ (userId INT, value BIGINT)";
    my $sql_prepTable_finalSep = 
"CREATE TEMPORARY TABLE finalSep (
    userName    VARCHAR(24),
    nFilesNow   INT NOT NULL,
    nJobsNow    INT NOT NULL,
    mbRead      BIGINT NOT NULL,
    nFiles      INT NOT NULL,
    nJobs       INT NOT NULL,
    timePeriod  VARCHAR(8) # hour, day, month, year
)";

    my $sql_dropTable_tt       = "DROP TABLE IF EXISTS tt";
    my $sql_dropTable_t_       = "DROP TABLE IF EXISTS t_";
    my $sql_dropTable_finalSep = "DROP TABLE IF EXISTS finalSep";

    my $sql_findNow_nFiles = 
"REPLACE INTO tt 
       SELECT userId
       FROM (SELECT userId,
                    COUNT(DISTINCT pathId) AS n
             FROM   rtOpenedSessions os, rtOpenedFiles of
             WHERE  os.id = of.sessionId
                    GROUP BY userId
                    ORDER BY n DESC
                    LIMIT $theLimit
            ) AS X";

    my $sql_findNow_nJobs = 
"REPLACE INTO tt 
       SELECT userId
       FROM (SELECT userId,
                    COUNT(DISTINCT CONCAT(pId, clientHId) ) AS n
             FROM   rtOpenedSessions os, rtOpenedFiles of
             WHERE  os.id = of.sessionId
                    GROUP BY userId
                    ORDER BY n DESC
                    LIMIT $theLimit
            ) AS X";

    my $sql_insert_rb_os = 
"INSERT INTO t_
       SELECT userId,
              SUM(bytesR)/(1024*1024) AS n
       FROM   rtOpenedSessions os, rtClosedFiles cf
       WHERE  os.id = cf.sessionId AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY userId
              ORDER BY n DESC
              LIMIT $theLimit";

    my $sql_insert_rb_cs = 
"INSERT INTO t_
       SELECT userId,
              SUM(bytesR)/(1024*1024) AS n
       FROM   rtClosedSessions os, rtClosedFiles cf
       WHERE  os.id = cf.sessionId AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY userId
              ORDER BY n DESC
              LIMIT $theLimit";

    my $sql_add_rb = 
"REPLACE INTO tt
        SELECT userId
        FROM (SELECT userId,
                     SUM(value) AS n
              FROM   t_
                     GROUP BY userId
                     ORDER BY n DESC
                     LIMIT $theLimit
             ) AS X";

    my $sql_findPast_nFiles = 
"CREATE TEMPORARY TABLE t_ (userId INT, value INT)";

    my $sql_insert_nFiles_os = 
"INSERT INTO t_
       SELECT userId,
              COUNT(DISTINCT pathId) AS n
       FROM   rtOpenedSessions os, rtClosedFiles cf
       WHERE  os.id = cf.sessionId AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY userId
              ORDER BY n DESC
              LIMIT $theLimit";

    my $sql_insert_nFiles_cs = 
"INSERT INTO t_
       SELECT userId,
              COUNT(DISTINCT pathId) AS n
       FROM   rtClosedSessions os, rtClosedFiles cf
       WHERE  os.id = cf.sessionId AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY userId
              ORDER BY n DESC
              LIMIT $theLimit";

    my $sql_add_nFiles = 
"REPLACE INTO tt
        SELECT userId
        FROM (SELECT userId,
                     SUM(value) AS n
              FROM   t_
                     GROUP BY userId
                     ORDER BY n DESC
                     LIMIT $theLimit
             ) AS X";

    my $sql_findPast_nJobs = 
"REPLACE INTO tt
       SELECT userId
       FROM (SELECT userId, 
                    COUNT(DISTINCT CONCAT(pId, clientHId) ) AS n
             FROM   rtClosedSessions os
             WHERE  disconnectT > DATE_SUB(NOW(), INTERVAL $theInterval)
                    GROUP BY userId
                    ORDER BY n DESC
                    LIMIT $theLimit
             ) AS X";

    my $sql_fillNow_nJobs = 
"INSERT INTO finalSep (userName, nJobsNow, timePeriod)
       SELECT userName, 
              COUNT(DISTINCT CONCAT(pId, clientHId) ),
              \"$theKeyword\"
       FROM   users, tt, rtOpenedSessions os
       WHERE  users.id = tt.userId  AND
              tt.userId = os.userId
              GROUP BY tt.userId";

    my $sql_fillNow_nFiles = 
"INSERT INTO finalSep (userName, nFilesNow, timePeriod)
       SELECT userName, 
              COUNT(DISTINCT pathId),
              \"$theKeyword\"
       FROM   users, tt, rtOpenedSessions os, rtOpenedFiles of
       WHERE  users.id = tt.userId  AND
              tt.userId = os.userId AND
              os.id = of.sessionId
              GROUP BY tt.userId";

    my $sql_fillPast_mbRead_os = 
"INSERT INTO finalSep (userName, mbRead, timePeriod)
       SELECT userName,
              SUM(bytesR)/(1024*1024) AS n,
              \"$theKeyword\"
       FROM   users, tt, rtOpenedSessions os, rtClosedFiles cf
       WHERE  users.id = tt.userId  AND
              tt.userId = os.userId AND
              os.id = cf.sessionId  AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY tt.userId";

    my $sql_fillPast_mbRead_cs = 
"INSERT INTO finalSep (userName, mbRead, timePeriod)
       SELECT userName,
              SUM(bytesR)/(1024*1024) AS n,
              \"$theKeyword\"
       FROM   users, tt, rtClosedSessions os, rtClosedFiles cf
       WHERE  users.id = tt.userId  AND
              tt.userId = os.userId AND
              os.id = cf.sessionId  AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY tt.userId";

    my $sql_fillPast_nFiles_os = 
"INSERT INTO finalSep (userName, nFiles, timePeriod)
       SELECT userName,
              COUNT(DISTINCT pathId),
              \"$theKeyword\"
       FROM   users, tt, rtOpenedSessions os, rtClosedFiles cf
       WHERE  users.id = tt.userId  AND
              tt.userId = os.userId AND
              os.id = cf.sessionId  AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY tt.userId";

    my $sql_fillPast_nFiles_cs = 
"INSERT INTO finalSep (userName, nFiles, timePeriod)
       SELECT userName,
              COUNT(DISTINCT pathId),
              \"$theKeyword\"
       FROM   users, tt, rtClosedSessions os, rtClosedFiles cf
       WHERE  users.id = tt.userId  AND
              tt.userId = os.userId AND
              os.id = cf.sessionId  AND
              closeT > DATE_SUB(NOW(), INTERVAL $theInterval)
              GROUP BY tt.userId";

    my $sql_fillPast_nJobs = 
"INSERT INTO finalSep (userName, nJobs, timePeriod)
       SELECT userName,
              COUNT(DISTINCT CONCAT(pId, clientHId) ) AS n,
              \"$theKeyword\"
       FROM   users, tt, rtClosedSessions cs
       WHERE  users.id = tt.userId AND
              tt.userId = cs.userId
              GROUP BY tt.userId";

my $sql_deleteOldData = 
"DELETE FROM topPerfUsers
        WHERE timePeriod LIKE \"$theKeyword\"";

    my $sql_compressIntoFinal = 
"INSERT INTO topPerfUsers
       SELECT userName,
              SUM(nFilesNow) AS nFilesNow,
              SUM(nJobsNow)  AS nJobsNow,
              SUM(mbRead)    AS mbRead,
              SUM(nFiles)    AS nFiles,
              SUM(nJobs)     AS nJobs,
              timePeriod
       FROM   finalSep
       WHERE  timePeriod LIKE \"$theKeyword\"
              GROUP BY userName
              ORDER BY mbRead DESC";

    my $sth_prepTable_tt       = $dbh->prepare($sql_prepTable_tt)       or die "\"$sql_prepTable_tt\", $DBI::errstr\n";
    my $sth_prepTable_t_       = $dbh->prepare($sql_prepTable_t_)       or die "\"$sql_prepTable_t_\", $DBI::errstr\n";
    my $sth_prepTable_finalSep = $dbh->prepare($sql_prepTable_finalSep) or die "\"$sql_prepTable_finalSep\", $DBI::errstr\n";
    my $sth_dropTable_tt       = $dbh->prepare($sql_dropTable_tt)       or die "\"$sql_dropTable_tt\", $DBI::errstr\n";
    my $sth_dropTable_t_       = $dbh->prepare($sql_dropTable_t_)       or die "\"$sql_dropTable_t_\", $DBI::errstr\n";
    my $sth_dropTable_finalSep = $dbh->prepare($sql_dropTable_finalSep) or die "\"$sql_dropTable_finalSep\", $DBI::errstr\n";
    my $sth_findNow_nFiles     = $dbh->prepare($sql_findNow_nFiles)     or die "\"$sql_findNow_nFiles\", $DBI::errstr\n";
    my $sth_findNow_nJobs      = $dbh->prepare($sql_findNow_nJobs)      or die "\"$sql_findNow_nJobs\", $DBI::errstr\n";
    my $sth_insert_rb_os       = $dbh->prepare($sql_insert_rb_os)       or die "\"$sql_insert_rb_os\", $DBI::errstr\n";
    my $sth_insert_rb_cs       = $dbh->prepare($sql_insert_rb_cs)       or die "\"$sql_insert_rb_cs\", $DBI::errstr\n";
    my $sth_add_rb             = $dbh->prepare($sql_add_rb)             or die "\"$sql_add_rb\", $DBI::errstr\n";
    my $sth_findPast_nFiles    = $dbh->prepare($sql_findPast_nFiles)    or die "\"$sql_findPast_nFiles\", $DBI::errstr\n";
    my $sth_insert_nFiles_os   = $dbh->prepare($sql_insert_nFiles_os)   or die "\"$sql_insert_nFiles_os\", $DBI::errstr\n";
    my $sth_insert_nFiles_cs   = $dbh->prepare($sql_insert_nFiles_cs)   or die "\"$sql_insert_nFiles_cs\", $DBI::errstr\n";
    my $sth_add_nFiles         = $dbh->prepare($sql_add_nFiles)         or die "\"$sql_add_nFiles\", $DBI::errstr\n";
    my $sth_findPast_nJobs     = $dbh->prepare($sql_findPast_nJobs)     or die "\"$sql_findPast_nJobs\", $DBI::errstr\n";
    my $sth_fillNow_nJobs      = $dbh->prepare($sql_fillNow_nJobs)      or die "\"$sql_fillNow_nJobs\", $DBI::errstr\n";
    my $sth_fillNow_nFiles     = $dbh->prepare($sql_fillNow_nFiles)     or die "\"$sql_fillNow_nFiles\", $DBI::errstr\n";
    my $sth_fillPast_mbRead_os = $dbh->prepare($sql_fillPast_mbRead_os) or die "\"$sql_fillPast_mbRead_os\", $DBI::errstr\n";
    my $sth_fillPast_mbRead_cs = $dbh->prepare($sql_fillPast_mbRead_cs) or die "\"$sql_fillPast_mbRead_cs\", $DBI::errstr\n";
    my $sth_fillPast_nFiles_os = $dbh->prepare($sql_fillPast_nFiles_os) or die "\"$sql_fillPast_nFiles_os\", $DBI::errstr\n";
    my $sth_fillPast_nFiles_cs = $dbh->prepare($sql_fillPast_nFiles_cs) or die "\"$sql_fillPast_nFiles_cs\", $DBI::errstr\n";
    my $sth_fillPast_nJobs     = $dbh->prepare($sql_fillPast_nJobs)     or die "\"$sql_fillPast_nJobs\", $DBI::errstr\n";
    my $sth_deleteOldData      = $dbh->prepare($sql_deleteOldData)      or die "\"$sql_deleteOldData\", $DBI::errstr\n";
    my $sth_compressIntoFinal  = $dbh->prepare($sql_compressIntoFinal)  or die "\"$sql_compressIntoFinal\", $DBI::errstr\n";

    $sth_prepTable_tt->execute()       or die "Failed to exec \"$sql_prepTable_tt\",       $DBI::errstr";
    $sth_prepTable_finalSep->execute() or die "Failed to exec \"$sql_prepTable_finalSep\", $DBI::errstr";
    $sth_findNow_nFiles->execute()     or die "Failed to exec \"$sql_findNow_nFiles\",     $DBI::errstr";
    $sth_findNow_nJobs->execute()      or die "Failed to exec \"$sql_findNow_nJobs\",      $DBI::errstr";
    $sth_dropTable_t_->execute()       or die "Failed to exec \"$sql_dropTable_t_\",       $DBI::errstr";
    $sth_prepTable_t_->execute()       or die "Failed to exec \"$sql_prepTable_t_\",       $DBI::errstr";
    $sth_insert_rb_os->execute()       or die "Failed to exec \"$sql_insert_rb_os\",       $DBI::errstr";
    $sth_insert_rb_cs->execute()       or die "Failed to exec \"$sql_insert_rb_cs\",       $DBI::errstr";
    $sth_add_rb->execute()             or die "Failed to exec \"$sql_add_rb\",             $DBI::errstr";
    $sth_dropTable_t_->execute()       or die "Failed to exec \"$sql_dropTable_t_\",       $DBI::errstr";
    $sth_prepTable_t_->execute()       or die "Failed to exec \"$sql_prepTable_t_\",       $DBI::errstr";
    $sth_insert_nFiles_os->execute()   or die "Failed to exec \"$sql_insert_nFiles_os\",   $DBI::errstr";
    $sth_insert_nFiles_cs->execute()   or die "Failed to exec \"$sql_insert_nFiles_cs\",   $DBI::errstr";
    $sth_add_nFiles->execute()         or die "Failed to exec \"$sql_add_nFiles\",         $DBI::errstr";
    $sth_dropTable_t_->execute()       or die "Failed to exec \"$sql_dropTable_t_\",       $DBI::errstr";
    $sth_findPast_nFiles->execute()    or die "Failed to exec \"$sql_findPast_nFiles\",    $DBI::errstr";
    $sth_findPast_nJobs->execute()     or die "Failed to exec \"$sql_findPast_nJobs\",     $DBI::errstr";
    $sth_fillNow_nJobs->execute()      or die "Failed to exec \"$sql_fillNow_nJobs\",      $DBI::errstr";
    $sth_fillNow_nFiles->execute()     or die "Failed to exec \"$sql_fillNow_nFiles\",     $DBI::errstr";
    $sth_fillPast_mbRead_os->execute() or die "Failed to exec \"$sql_fillPast_mbRead\",    $DBI::errstr";
    $sth_fillPast_mbRead_cs->execute() or die "Failed to exec \"$sql_fillPast_mbRead\",    $DBI::errstr";
    $sth_fillPast_nFiles_os->execute() or die "Failed to exec \"$sql_fillPast_nFiles_os\", $DBI::errstr";
    $sth_fillPast_nFiles_cs->execute() or die "Failed to exec \"$sql_fillPast_nFiles_cs\", $DBI::errstr";
    $sth_fillPast_nJobs->execute()     or die "Failed to exec \"$sql_fillPast_nJobs\",     $DBI::errstr";
    $sth_deleteOldData->execute()      or die "Failed to exec \"$sql_deleteOldData\",      $DBI::errstr";
    $sth_compressIntoFinal->execute()  or die "Failed to exec \"$sql_compressIntoFinal\",  $DBI::errstr";
    $sth_dropTable_tt->execute()       or die "Failed to exec \"$sql_dropTable_tt\",       $DBI::errstr";
    $sth_dropTable_finalSep->execute() or die "Failed to exec \"$sql_dropTable_finalSep\", $DBI::errstr";
}
  
