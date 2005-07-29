#!/usr/local/bin/perl -w

use DBI;

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


$initFlag  = 1;
$updInt    = 60;     # update interval
$dbName    = 'xrdmon_new'; # FIXME
$mySQLUser = 'becla';      # FIXME

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


$cycleEndTime = time() + $updInt;

&doInitialization();
 
#start an infinite loop
while ( 1 ) {
    &doUpdate();

    my $sec2Sleep = 60 - (localtime)[0];
    print "sleeping $sec2Sleep sec... \n";
    sleep $sec2Sleep;
}

###############################################################################
###############################################################################
###############################################################################

sub doInitialization() {

    @primes = (101, 127, 157, 181, 199, 223, 239, 251, 271, 307);

    # do the necessary one-time initialization
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	return;
    }

    @siteNames = &runQueryRetArray("SELECT name FROM sites");
    foreach $siteName (@siteNames) {
	$siteIds{$siteName} = &runQueryWithRet("SELECT id 
                                                FROM sites 
                                                WHERE name = \"$siteName\"");
    }

    my ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastHour");
    if ( $lastTime ) {
        ($nMin, $lastNoJobs, $lastNoUsers, $lastNoUniqueF, $lastNoNonUniqueF) =
          &runQueryWithRet("SELECT seqNo,noJobs,noUsers,noUniqueF,noNonUniqueF
                            FROM   statsLastHour 
                            WHERE  date = \"$lastTime\"");
        $nMin += 1;
    } else { 
        $nMin=$lastNoJobs=$lastNoUsers=$lastNoUniqueF=$lastNoNonUniqueF=0;
    }

    ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastDay");
    if ( $lastTime ) {
        $nHour = &runQueryWithRet("SELECT seqNo 
                                   FROM statsLastDay
                                   WHERE date = \"$lastTime\"");
        $nHour += 1;
    } else { 
        $nHour = 0;
    }
    ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastWeek");
    if ( $lastTime ) {
        $nWDay = &runQueryWithRet("SELECT seqNo 
                                   FROM statsLastWeek 
                                   WHERE date = \"$lastTime\"");
        $nWDay += 1;
    } else { 
        $nWDay = 0;
    }

    ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM statsLastMonth");
    if ( $lastTime ) {
        $nDay = &runQueryWithRet("SELECT seqNo 
                                  FROM statsLastMonth 
                                  WHERE date = \"$lastTime\"");
        $nDay += 1;
    } else {
        $nDay = 0;
    }
    $bbkListSize = 250;
    $minSizeLoadTime = 5;
    $initFlag = 0;

    $dbh->disconnect();
}



sub doUpdate {
    my $ts = &timestamp();

    # connect to the database
    print "\n$ts Connecting to database...\n";
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	return;
    }

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

    foreach $siteName (@siteNames) {
	updateOneSite($siteName, 
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
    print "$ts All done.\n";

}

sub updateOneSite() {
    my ($siteName, $loadTime, 
        $loadLastHour, $loadLastDay, 
        $loadLastWeek, $loadLastMonth, $loadAllMonths) = @_;

    print "Updating data for --> $siteName <--\n";

    if ( $loadLastHour  == 1 ) { &loadStatsLastHour($siteName, $loadTime)  };
    if ( $loadLastDay   == 1 ) { &loadStatsLastDay($siteName, $loadTime)   };
    if ( $loadLastWeek  == 1 ) { &loadStatsLastWeek($siteName, $loadTime)  };
    if ( $loadLastMonth == 1 ) { &loadStatsLastMonth($siteName, $loadTime) };
    if ( $loadAllMonths == 1 ) { &loadStatsAllMonths($siteName, $loadTime) };

    &reloadTopPerfTables($siteName);

    # load file sizes
    if       ( ! ($nMin % $loadFreq3rdFail) ) {
        &loadFileSizes( $siteName, -3 );
    } elsif ( ! ($nMin % $loadFreq2ndFail) ) {
        &loadFileSizes( $siteName, -2 );
    } elsif ( ! ($nMin % $loadFreq1stFail) ) {
        &loadFileSizes( $siteName, -1 );
    } else {
        &loadFileSizes( $siteName, 0 ); # every minute
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
    my $sec  = (localtime)[0];
    my $min  = (localtime)[1];
    my $hour = (localtime)[2];
    my $day  = (localtime)[3];
    my $month  = (localtime)[4] + 1;
    my $year = (localtime)[5] + 1900;

    return sprintf("%04d-%02d-%02d %02d:%02d:%02d", $year, $month, $day, $hour, $min, $sec);
}

sub loadFileSizes() {

    print "Loading file sizes... \n";
    use vars qw($sizeIndex $fromId $toId $path $size @files @inBbk);
    my ($siteName, $sizeIndex) = @_;
    &runQuery("CREATE TEMPORARY TABLE zerosize  (name VARCHAR(255), id MEDIUMINT UNSIGNED, hash MEDIUMINT)");
    &runQuery("INSERT INTO zerosize(name, id, hash) SELECT name, id, hash FROM paths 
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
           if ( $id ) {
	       &runQuery("UPDATE paths SET size = $size WHERE id = $id ");
	   }
       }
       # decrement size by 1 for files that failed bbk.
       foreach $path ( @files ) {
           if ( ! grep { $_ eq $path } @inBbk ) {
               my $hashValue = &returnHash("$path");
               my $id = &runQueryWithRet("SELECT id FROM zerosize 
                                          WHERE hash = $hashValue AND name = '$path'");
               if ( $id ) {
		   &runQuery("UPDATE paths SET size = size - 1 WHERE id = $id ");
	       }
           }
       }
       print " done ", scalar @files, " files updated. Update time = ", time() - $t0, " s \n";
       last if ( @files < $bbkListSize );
       $skip += $bbkListSize;
    }
    &runQuery("DROP TABLE IF EXISTS zerosize");
}


sub loadStatsLastHour() {
    my ($siteName, $loadTime) = @_;

    use vars qw($seqNo $noJobs $noUsers $noUniqueF $noNonUniqueF $deltaJobs $jobs_p 
                $deltaUsers $users_p $deltaUniqueF $uniqueF_p $deltaNonUniqueF $nonUniqueF_p);
    $seqNo = $nMin % 60;
    my $siteId = $siteIds{$siteName};

    &runQuery("DELETE FROM statsLastHour WHERE seqNo = $seqNo AND siteId = $siteId");
    ($noJobs, $noUsers) = &runQueryWithRet("SELECT COUNT(DISTINCT pId, clientHId), COUNT(DISTINCT userId) 
                                                FROM ${siteName}_openedSessions");

    ($noUniqueF, $noNonUniqueF) = &runQueryWithRet("SELECT COUNT(DISTINCT pathId), COUNT(*) 
                                                        FROM ${siteName}_openedFiles");
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
    my ($siteName, $loadTime) = @_;

    my $seqNo = $nHour % 24;
    my $siteId = $siteIds{$siteName};

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
    my ($siteName, $loadTime) = @_;
    print "called loadStatsLastWeek\n";
    my $seqNo = $nWDay % 7;
    my $siteId = $siteIds{$siteName};

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
    my ($siteName, $loadTime) = @_;

    my $seqNo = $nDay % 31;
    my $siteId = $siteIds{$siteName};

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
    my ($siteName, $loadTime) = @_;

    my $siteId = $siteIds{$siteName};

    # note that (localtime)[4] returns month in the range 0 - 11 and normally should be
    # increased by 1 to show the current month.
    $lastMonth = (localtime)[4];

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                 MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                 MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                          FROM   statsLastMonth
                          WHERE  MNTH(date) = \"$lastMonth\"
                            AND  siteId = $siteId"); 

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
    my ($siteName) = @_;

    # create tables
    &runQuery("CREATE TEMPORARY TABLE jj  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE ff  (theId INT, n INT, s INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE uu  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE vv  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE xx  (theId INT UNIQUE KEY, INDEX (theId))");
    @tables = ("jj", "ff", "uu", "vv", "xx");

    # run queries for past (last hour/day/week/month/year
    &runQueries4AllTopPerfTablesPast("Hour", 20, $siteName);
    if ( $dCounter == $dUpdatesFreq ) {
	&runQueries4AllTopPerfTablesPast("Day", 20, $siteName);
	$dCounter = 0;
    } else {
	$dCounter += 1;
    }
    if ( $wCounter == $wUpdatesFreq ) {
	&runQueries4AllTopPerfTablesPast("Week", 20, $siteName);
	$wCounter = 0;
    } else {
	$wCounter += 1;
    }
    if ( $mCounter == $mUpdatesFreq ) {
	&runQueries4AllTopPerfTablesPast("Month", 20, $siteName);
	$mCounter = 0;
    } else {
	$mCounter += 1;
    }
    if ( $yCounter == $yUpdatesFreq ) {
	&runQueries4AllTopPerfTablesPast("Year", 20, $siteName);
	$yCounter = 0;
    } else {
	$yCounter += 1;
    }
    # run queries for now
    &runQueries4AllTopPerfTablesNow(20, $siteName);

    # delete tables
    foreach $table (@tables) { &runQuery("DROP TABLE IF EXISTS $table"); }
}

sub runQueries4AllTopPerfTablesPast() {
    my ($theKeyword, $theLimit, $siteName) = @_;

    &runTopUsrFsQueriesPast($theKeyword, $theLimit, "USERS", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesPast($theKeyword, $theLimit, "SKIMS", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesPast($theKeyword, $theLimit, "TYPES", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopUsrFsQueriesPast($theKeyword, $theLimit, "FILES", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
}

sub runQueries4AllTopPerfTablesNow() {
    my ($theLimit, $siteName) = @_;

    &runTopUsrFsQueriesNow($theLimit, "USERS", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesNow($theLimit, "SKIMS", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesNow($theLimit, "TYPES", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopUsrFsQueriesNow($theLimit, "FILES", $siteName);
    foreach $table (@tables) { &runQuery("TRUNCATE TABLE $table"); }
}

sub runTopUsrFsQueriesPast() {
    my ($theKeyword, $theLimit, $what, $siteName) = @_;

    print "# updating topPerf $what tables for $theKeyword\n";

    my $idInTable        = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "USERS" ) {
        $idInTable        = "userId";
        $destinationTable = "${siteName}_topPerfUsersPast";
    } elsif ( $what eq "FILES" ) {
        $idInTable        = "pathId";
        $destinationTable = "${siteName}_topPerfFilesPast";
    } else {
        die "Invalid arg, expected USERS or FILES\n";
    }

    # past jobs
    if ( $what eq "USERS" ) {
        &runQuery("INSERT INTO jj
            SELECT $idInTable, 
                   COUNT(DISTINCT pId, clientHId) AS n
            FROM   ${siteName}_closedSessions_Last$theKeyword
            GROUP BY $idInTable");
    } elsif ( $what eq "FILES" ) {
        &runQuery("INSERT INTO jj
            SELECT $idInTable, 
                   COUNT(DISTINCT pId, clientHId) AS n
            FROM   ${siteName}_closedSessions_Last$theKeyword cs,
                   ${siteName}_closedFiles_Last$theKeyword cf
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
                             FROM   ${siteName}_openedSessions os,
                                    ${siteName}_closedFiles_Last$theKeyword cf,
                                    paths p
                             WHERE  os.id = cf.sessionId 
                                AND cf.pathId = p.id 
                             UNION ALL
                             SELECT $idInTable, pathId, size 
                             FROM   ${siteName}_closedSessions_Last$theKeyword cs,
                                    ${siteName}_closedFiles_Last$theKeyword cf,
                                    paths p
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
                 FROM   ${siteName}_openedSessions os,
                        ${siteName}_closedFiles_Last$theKeyword cf
                 WHERE  os.id = cf.sessionId
                 UNION ALL
                 SELECT $idInTable, bytesR
                 FROM   ${siteName}_closedSessions_Last$theKeyword cs,
                        ${siteName}_closedFiles_Last$theKeyword cf
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
    my ($theLimit, $what, $siteName) = @_;

    print "# updating topPerf $what tables for NOW\n";

    my $idInTable        = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "USERS" ) {
        $idInTable        = "userId";
        $destinationTable = "${siteName}_topPerfUsersNow";
        $pastTable        = "${siteName}_topPerfUsersPast";
    } elsif ( $what eq "FILES" ) {
        $idInTable        = "pathId";
        $destinationTable = "${siteName}_topPerfFilesNow";
        $pastTable        = "${siteName}_topPerfFilesPast";
    } else {
        die "Invalid arg, expected USERS or FILES\n";
    }

    if ( $what eq "USERS" ) {
        # now jobs
        &runQuery("INSERT INTO jj
           SELECT $idInTable, COUNT(DISTINCT pId, clientHId ) AS n
        FROM   ${siteName}_openedSessions
        GROUP BY $idInTable");

        # now files
        &runQuery ("INSERT INTO ff 
        SELECT tmp.$idInTable, 
               COUNT(tmp.pathId) AS n,
               SUM(tmp.size)/(1024*1024) AS s
        FROM   (SELECT DISTINCT $idInTable, pathId, size
                FROM   ${siteName}_openedSessions os,
                       ${siteName}_openedFiles of,
                       paths p
                WHERE  os.id = of.sessionId
                   AND of.pathId = p.id
               ) AS tmp
        GROUP BY tmp.$idInTable");
    }
    if ( $what eq "FILES" ) {
        # now jobs
        &runQuery("INSERT INTO jj
            SELECT $idInTable, COUNT(DISTINCT pId, clientHId ) AS n
            FROM   ${siteName}_openedSessions os,
                   ${siteName}_openedFiles of
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
    my ($theKeyword, $theLimit, $what, $siteName) = @_;

    print "# updating topPerf $what tables for $theKeyword\n";

    my $idInPathTable    = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "SKIMS" ) {
        $idInPathTable    = "skimId";
        $destinationTable = "${siteName}_topPerfSkimsPast";
    } elsif ( $what eq "TYPES" ) {
        $idInPathTable    = "typeId";
        $destinationTable = "${siteName}_topPerfTypesPast";
    } else {
        die "Invalid arg, expected SKIMS or TYPES\n";
    }

    # past jobs
    &runQuery("REPLACE INTO jj
        SELECT $idInPathTable, 
               COUNT(DISTINCT pId, clientHId ) AS n
        FROM   ${siteName}_closedSessions_Last$theKeyword cs,
               ${siteName}_closedFiles_Last$theKeyword cf,
               paths p
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
                        FROM   ${siteName}_openedSessions os,
                               ${siteName}_closedFiles_Last$theKeyword cf,
                               paths p
                        WHERE  os.id = cf.sessionId
                           AND cf.pathId = p.id
                        UNION ALL
                        SELECT $idInPathTable, pathId, size
                        FROM   ${siteName}_closedSessions_Last$theKeyword cs,
                               ${siteName}_closedFiles_Last$theKeyword cf,
                               paths p
                        WHERE  cs.id = cf.sessionId
                           AND cf.pathId = p.id
                      ) AS oc
                ) AS tmp
        GROUP BY tmp.$idInPathTable");



    # past users
    &runQuery("REPLACE INTO uu
        SELECT $idInPathTable, 
               COUNT(DISTINCT userId) AS n
        FROM   ${siteName}_closedSessions_Last$theKeyword cs,
               ${siteName}_closedFiles_Last$theKeyword cf,
               paths p
        WHERE  cs.id = cf.sessionId
          AND cf.pathId = p.id
        GROUP BY $idInPathTable");

    # past volume - through opened & closed sessions
    &runQuery("INSERT INTO vv
         SELECT oc.$idInPathTable, 
                SUM(oc.bytesR)/(1024*1024)
         FROM   ( SELECT $idInPathTable, bytesR
                  FROM   ${siteName}_openedSessions os,
                         ${siteName}_closedFiles_Last$theKeyword cf,
                         paths p
                  WHERE  os.id = cf.sessionId
                    AND cf.pathId = p.id
                  UNION ALL
                  SELECT $idInPathTable, bytesR
                  FROM   ${siteName}_closedSessions_Last$theKeyword cs,
                         ${siteName}_closedFiles_Last$theKeyword cf,
                         paths p
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
    my ($theLimit, $what, $siteName) = @_;

    print "# updating topPerf $what tables for NOW\n";

    my $idInPathTable    = "INVALID";
    my $destinationTable = "INVALID";

    if ( $what eq "SKIMS" ) {
        $idInPathTable    = "skimId";
        $destinationTable = "${siteName}_topPerfSkimsNow";
        $pastTable        = "${siteName}_topPerfSkimsPast";
    } elsif ( $what eq "TYPES" ) {
        $idInPathTable    = "typeId";
        $destinationTable = "${siteName}_topPerfTypesNow";
        $pastTable        = "${siteName}_topPerfTypesPast";
    } else {
        die "Invalid arg, expected SKIMS or TYPES\n";
    }

    # now jobs
    &runQuery("INSERT INTO jj
        SELECT $idInPathTable,
               COUNT(DISTINCT pId, clientHId ) AS n
        FROM   ${siteName}_openedSessions os,
               ${siteName}_openedFiles of,
               paths p
        WHERE  os.id = of.sessionId
           AND of.pathId = p.id
        GROUP BY $idInPathTable");

    # now files
    &runQuery("REPLACE INTO ff 
        SELECT tmp.$idInPathTable,
               COUNT(tmp.pathId) AS n,
               SUM(tmp.size)/(1024*1024)  AS s
        FROM   ( SELECT DISTINCT $idInPathTable, pathId, size
                 FROM   ${siteName}_openedSessions os,
                        ${siteName}_openedFiles of,
                        paths p
                 WHERE  os.id = of.sessionId
                    AND of.pathId = p.id
               ) AS tmp
        GROUP BY tmp.$idInPathTable");

    # now users
    &runQuery("REPLACE INTO uu 
        SELECT $idInPathTable,
               COUNT(DISTINCT userId) AS n
        FROM   ${siteName}_openedSessions os,
               ${siteName}_openedFiles of,
               paths p
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
