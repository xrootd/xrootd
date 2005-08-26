#!/usr/local/bin/perl -w

use DBI;

###############################################################################
#                                                                             #
#                             prepareMySQLStats.pl                            #
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
while ( $_ = <INFILE> ) {
    chomp();
    my ($token, $v1) = split(/ /, $_);
    if ( $token =~ "dbName:" ) {
        $dbName = $v1;
    } elsif ( $token =~ "MySQLUser:" ) {
        $mySQLUser = $v1;
    } elsif ( $token =~ "MySQLSocket:" ) {
        $mysqlSocket = $v1;
    } else {
        print "Invalid entry: \"$_\"\n";
        close INFILE;
        exit;
    }
}
close INFILE;


# connect to the database
unless ( $dbh = DBI->connect("dbi:mysql:$dbName;mysql_socket=$mysqlSocket",$mySQLUser) ) {
    print "Error while connecting to database. $DBI::errstr\n";
    exit;
}

&doInitialization();

$dbh->disconnect();

 
#start an infinite loop
while ( 1 ) {

    # connect to the database
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName;mysql_socket=$mysqlSocket",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	exit;
    }

    my $loadTime = &timestamp();
    print "Current time is $loadTime. Doing HOUR update (every minute)\n";

    $loadTime = &gmtimestamp();
    my @gmt   = gmtime(time());
    my $sec   = $gmt[0];
    my $min   = $gmt[1];
    my $hour  = $gmt[2];
    my $day   = $gmt[3];
    my $wday  = $gmt[6];
    my $yday  = $gmt[7];

    # create temporary tables for topPerformers
    &runQuery("CREATE TEMPORARY TABLE jj  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE ff  (theId INT, n INT, s INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE uu  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE vv  (theId INT, n INT, INDEX (theId))");
    &runQuery("CREATE TEMPORARY TABLE xx  (theId INT UNIQUE KEY, INDEX (theId))");
    @topPerfTables = ("jj", "ff", "uu", "vv", "xx");

    foreach $siteName (@siteNames) {
	&prepareStats4OneSite($siteName, 
			      $loadTime,
			      $sec,
			      $min,
			      $hour,
			      $day,
 			      $wday,
			      $yday);
    }

    $dbh->disconnect();

    # wake up every minute at HH:MM:30
    my $sec2Sleep = 90 - $sec;
    if ( $sec < 30 ) {
	$sec2Sleep -= 60;
    }
        
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

sub doInitialization() {
    use vars qw($lastTime  $siteId);
    $stopFName = "$confFile.stop";
    @primes = (101, 127, 157, 181, 199, 223, 239, 251, 271, 307);

    # cleanup old entries
    $GMTnow = &gmtimestamp();
    &runQuery("DELETE FROM statsLastHour  WHERE date < DATE_SUB('$GMTnow', INTERVAL  1 HOUR)");
    &runQuery("DELETE FROM statsLastDay   WHERE date < DATE_SUB('$GMTnow', INTERVAL  24 HOUR)");
    &runQuery("DELETE FROM statsLastWeek  WHERE date < DATE_SUB('$GMTnow', INTERVAL 168 HOUR)");
    &runQuery("DELETE FROM statsLastMonth WHERE date < DATE_SUB('$GMTnow', INTERVAL 30  DAY)");
    &runQuery("DELETE FROM statsLastYear  WHERE date < DATE_SUB('$GMTnow', INTERVAL 365 DAY)");


    # find all sites
    @siteNames = &runQueryRetArray("SELECT name FROM sites");
    foreach $siteName (@siteNames) {
	$siteId = &runQueryWithRet("SELECT id 
                                      FROM sites 
                                     WHERE name = \"$siteName\"");
        $siteIds{$siteName} = $siteId;

        $lastTime = &runQueryWithRet("SELECT MAX(date) FROM statsLastHour
                                       WHERE siteId = $siteId ");
        if ( $lastTime ) {
	     ($lastNoJobs[$siteId],    $lastNoUsers[$siteId], 
              $lastNoUniqueF[$siteId], $lastNoNonUniqueF[$siteId]) 
	      = &runQueryWithRet("SELECT noJobs,noUsers,noUniqueF,noNonUniqueF
                              FROM   statsLastHour 
                              WHERE  date   = \"$lastTime\"  AND
                                     siteId = $siteId           ");
        } else { 
              $lastNoJobs[$siteId]=$lastNoUsers[$siteId]=0;
              $lastNoUniqueF[$siteId]=$lastNoNonUniqueF[$siteId]=0;
        }
    }

    # some other stuff
    $bbkListSize = 250;
    $minSizeLoadTime = 5;
}


sub prepareStats4OneSite() {
    my ($siteName, $loadTime, $sec, $min, $hour, $day, $wday, $yday) = @_;

    print "Updating data for --> $siteName <--\n";

    # every min at HH:MM:30
    &runQuery("DELETE FROM ${siteName}_closedSessions_LastHour  
                     WHERE disconnectT < DATE_SUB(\"$loadTime\", INTERVAL  1 HOUR)");
    &runQuery("DELETE FROM ${siteName}_closedFiles_LastHour     
                     WHERE      closeT < DATE_SUB(\"$loadTime\", INTERVAL  1 HOUR)");
    &loadStatsLastHour($siteName, $loadTime, $min % 60);
    &loadTopPerfPast("Hour", 20, $siteName);


    if ( $min == 0 || $min == 15 || $min == 30 || $min == 45 ) {
	# every 15 min at HH:00:30, HH:15:30, HH:30:30, HH:45:30
        &runQuery("DELETE FROM ${siteName}_closedSessions_LastDay  
                         WHERE disconnectT < DATE_SUB(\"$loadTime\", INTERVAL  1 DAY)");
        &runQuery("DELETE FROM ${siteName}_closedFiles_LastDay     
                         WHERE      closeT < DATE_SUB(\"$loadTime\", INTERVAL  1 DAY)");
	&loadStatsLastPeriod($siteName, statsLastHour, statsLastDay, $loadTime, $hour*4+$min/15, 1);
	&loadTopPerfPast("Day", 20, $siteName);
    }

    if ( $min == 5 ) {
	# every hour at HH:05:30
        &runQuery("DELETE FROM ${siteName}_closedSessions_LastWeek  
                         WHERE disconnectT < DATE_SUB(\"$loadTime\", INTERVAL  7 DAY)");
        &runQuery("DELETE FROM ${siteName}_closedFiles_LastWeek     
                         WHERE      closeT < DATE_SUB(\"$loadTime\", INTERVAL  7 DAY)");
	&loadStatsLastPeriod($siteName, statsLastHour, statsLastWeek, $loadTime, $wday*24+$hour, 7);
	&loadTopPerfPast("Week", 20, $siteName);
    }

    if ( $min == 20 ) {
	if ( $hour == 0 || $hour == 6 || $hour == 12 || $hour == 18 ) {
	    # every 6 hours at 00:20:30, 06:20:30, 12:20:30, 18:20:30
            &runQuery("DELETE FROM ${siteName}_closedSessions_LastMonth  
                             WHERE disconnectT < DATE_SUB(\"$loadTime\", INTERVAL 30 DAY)");
            &runQuery("DELETE FROM ${siteName}_closedFiles_LastMonth     
                             WHERE      closeT < DATE_SUB(\"$loadTime\", INTERVAL 30 DAY)");
	    &loadStatsLastPeriod($siteName, statsLastDay, statsLastMonth, $loadTime, $day*4+$hour/6, 30);
	    &loadTopPerfPast("Month", 20, $siteName);

	}
	if ( $hour == 23 ) {
	    # every 24 hours at 23:20:30
            &runQuery("DELETE FROM ${siteName}_closedSessions_LastYear  
                             WHERE disconnectT < DATE_SUB(\"$loadTime\", INTERVAL 365 DAY)");
            &runQuery("DELETE FROM ${siteName}_closedFiles_LastYear     
                             WHERE      closeT < DATE_SUB(\"$loadTime\", INTERVAL 365 DAY)");
	    &loadStatsLastPeriod($siteName, statsLastDay, statsLastYear $loadTime, $yday, 365);
	    &loadTopPerfPast("Year", 20, $siteName);
	}
        if ( $hour == 1 && $wday == 1 ) {
            # every Sunday at 01:20:30
            &loadStatsAllYears($siteName, $loadTime);
	}
    }
    if ( $min == 40 ) {
        if ( $hour == 11 || $hour == 23 ) {
            # every 12 hours at 11:40:30, 23:40:30 
            &closeOpenedFiles($siteName, $loadTime);
	}
    }

    # top Perf - "now"
    &loadTopPerfNow(20, $siteName);

    # truncate temporary tables tables
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE $table"); }

    # load file sizes
    if ( $min == 37 ) {
	if ( $hour == 23 ) {
             # every 24 hours at 23:37:30
	    &loadFileSizes( $siteName, -3 );
	}
	if ( $hour == 11 || $hour == 23 ) {
            # every 12 hours at 11:37:30 and 23:37:30
	    &loadFileSizes( $siteName, -2 );
	}
        # every hour at HH:37:30
        &loadFileSizes( $siteName, -1 );
    }
    # every min at HH:MM:30
    &loadFileSizes( $siteName, 0 );
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



sub loadFileSizes() {
    my ($siteName, $sizeIndex) = @_;
    print "Loading file sizes... \n";
    use vars qw($sizeIndex $fromId $toId $path $size @files @inBbk);
    &runQuery("CREATE TEMPORARY TABLE zerosize  
               (name VARCHAR(255), id MEDIUMINT UNSIGNED, hash MEDIUMINT)");
    &runQuery("INSERT INTO zerosize(name, id, hash)
                      SELECT name, id, hash FROM paths 
                      WHERE  size BETWEEN $sizeIndex AND 0 
                      ORDER BY id 
                      LIMIT 7500");
     
    $skip = 0;
    while () {
	my $t0 = time();
	my $sec = (localtime)[0];
	my $timeLeft = 90-$sec;
	if ( $sec < 30 ) {
	    $timeLeft -= 60;
	}
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
       @bbkOut   = `BbkUser --lfn-file=bbkInput --quiet                 lfn bytes`;
       @bbkOut18 = `BbkUser --lfn-file=bbkInput --quiet --dbname=bbkr18 lfn bytes`;
       @bbkOut   = (@bbkOut, @bbkOut18);
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
    my ($siteName, $loadTime, $seqNo) = @_;

    use vars qw($noJobs $noUsers $noUniqueF $noNonUniqueF $deltaJobs $jobs_p 
                $deltaUsers $users_p $deltaUniqueF $uniqueF_p $deltaNonUniqueF $nonUniqueF_p);

    my $siteId = $siteIds{$siteName};

    &runQuery("DELETE FROM statsLastHour WHERE date < DATE_SUB(\"$loadTime\", INTERVAL  1 HOUR) AND
                                               siteId = $siteId     ");
    ($noJobs, $noUsers) = &runQueryWithRet("SELECT COUNT(DISTINCT pId, clientHId), COUNT(DISTINCT userId) 
                                              FROM ${siteName}_openedSessions");

    ($noUniqueF, $noNonUniqueF) = &runQueryWithRet("SELECT COUNT(DISTINCT pathId), COUNT(*) 
                                                      FROM ${siteName}_openedFiles");
    &runQuery("REPLACE INTO statsLastHour 
                      (seqNo, siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF) 
               VALUES ($seqNo, $siteId, \"$loadTime\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF)");

    $deltaJobs = $noJobs - $lastNoJobs[$siteId]; 
    $jobs_p = $lastNoJobs[$siteId] > 0 ? &roundoff( 100 * $deltaJobs / $lastNoJobs[$siteId] ) : -1;
    $deltaUsers = $noUsers - $lastNoUsers[$siteId];
    $users_p = $lastNoUsers[$siteId] > 0 ? &roundoff( 100 * $deltaUsers / $lastNoUsers[$siteId] ) : -1;
    $deltaUniqueF = $noUniqueF - $lastNoUniqueF[$siteId];
    $uniqueF_p = $lastNoUniqueF[$siteId] > 0 ? &roundoff( 100 * $deltaUniqueF / $lastNoUniqueF[$siteId] ) : -1;
    $deltaNonUniqueF = $noNonUniqueF - $lastNoNonUniqueF[$siteId];
    $nonUniqueF_p = $lastNoNonUniqueF[$siteId] > 0 ? &roundoff( 100 * $deltaNonUniqueF / $lastNoNonUniqueF[$siteId] ) : -1;
    &runQuery("REPLACE INTO rtChanges 
                           (siteId, jobs, jobs_p, users, users_p, uniqueF, uniqueF_p, 
                            nonUniqueF, nonUniqueF_p, lastUpdate)
                    VALUES ($siteId, $deltaJobs, $jobs_p, $deltaUsers, $users_p, $deltaUniqueF,
                            $uniqueF_p, $deltaNonUniqueF, $nonUniqueF_p, \"$loadTime\")");
    $lastNoJobs[$siteId]       = $noJobs;
    $lastNoUsers[$siteId]      = $noUsers;
    $lastNoUniqueF[$siteId]    = $noUniqueF;
    $lastNoNonUniqueF[$siteId] = $noNonUniqueF;
}


sub loadStatsAllYears() {
    my ($siteName, $loadTime) = @_;

    my $siteId = $siteIds{$siteName};

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                 MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                 MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                          FROM   statsLastWeek
                          WHERE  siteId = $siteId"); 

    &runQuery("INSERT INTO statsAllYears
                          (siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                    VALUES ($siteId, \"$loadTime\", $noJobs, $noUsers,
                            $noUniqueF, $noNonUniqueF, 
                            $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                            $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");
}

sub loadStatsLastPeriod() {
    use vars qw($noJobs   $noUsers   $noUniqueF   $noNonUniqueF    
                $minJobs  $minUsers  $minUniqueF  $minNonUniqueF  
                $maxJobs  $maxUsers  $maxUniqueF  $maxNonUniqueF);
    my ($siteName, $sourceTable, $targetTable, $loadTime, $seqNo, $tableInt) = @_;

    my $siteId = $siteIds{$siteName};

    my ($lastTime) = &runQueryWithRet("SELECT MAX(date) FROM $targetTable WHERE siteId = $siteId");    
    &runQuery("DELETE FROM $targetTable WHERE date < DATE_SUB(\"$loadTime\", INTERVAL $tableInt DAY )  AND 
                                              siteId = $siteId");

    if ( $lastTime ) {
        ($noJobs,  $noUsers,  $noUniqueF,  $noNonUniqueF, 
         $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
         $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)
        = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                   MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                   MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                              FROM $sourceTable 
                             WHERE siteId = $siteId        AND
                                     date > \"$lastTime\"     ");
    } else {
        ($noJobs,  $noUsers,  $noUniqueF,  $noNonUniqueF, 
         $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
         $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)
        = &runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                   MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                   MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                              FROM $sourceTable 
                             WHERE siteId = $siteId ");
    }


    &runQuery("REPLACE INTO $targetTable
                          (seqNo, siteId, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                    VALUES ($seqNo, $siteId, \"$loadTime\", $noJobs, $noUsers,
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


# closes opened files corresponding to closed sessions
sub closeOpenedFiles() {
    my ($siteName, $GMTnow) = @_;

    print "Closing opened files for closed sessions\n";
    &runQuery("CREATE TEMPORARY TABLE xcf like SLAC_closedFiles");

    # give it an extra hour: sometimes closeFile
    # info may arrive after the closeSession info
    &runQuery("INSERT INTO xcf
                 SELECT of.id,
                        of.sessionId,
                        of.pathId,
                        of.openT,
                        cs.disconnectT,
                        -1,
                        -1
                 FROM   ${siteName}_openedFiles of,
                        ${siteName}_closedSessions cs
                 WHERE  sessionId = cs.id
                    AND disconnectT < DATE_SUB('$GMTnow', INTERVAL 1 HOUR)");

    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles
                 SELECT * FROM xcf");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastYear
                 SELECT * FROM xcf");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastMonth
                 SELECT * FROM xcf WHERE openT > DATE_SUB('$GMTnow', INTERVAL 1 MONTH)");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastWeek
                 SELECT * FROM xcf WHERE openT > DATE_SUB('$GMTnow', INTERVAL 1 WEEK)");
    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_LastDay
                 SELECT * FROM xcf WHERE openT > DATE_SUB('$GMTnow', INTERVAL 1 DAY)");

    &runQuery("DELETE FROM  ${siteName}_openedFiles
               USING ${siteName}_openedFiles, xcf
               WHERE ${siteName}_openedFiles.id = xcf.id");
    my $noDone = &runQueryWithRet("SELECT COUNT(*) FROM xcf");
    &runQuery("DROP TABLE xcf");
    print "Forced close $noDone files\n";
}


#######################################################
### everything below is for loading top perf tables ###
#######################################################

sub loadTopPerfPast() {
    my ($theKeyword, $theLimit, $siteName) = @_;

    &runTopUsersQueriesPast($theKeyword, $theLimit, $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesPast($theKeyword, $theLimit, "SKIMS", $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesPast($theKeyword, $theLimit, "TYPES", $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopFilesQueriesPast($theKeyword, $theLimit, $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
}

sub loadTopPerfNow() {
    my ($theLimit, $siteName) = @_;

    &runTopUsersQueriesNow($theLimit, $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesNow($theLimit, "SKIMS", $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopSkimsQueriesNow($theLimit, "TYPES", $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
    &runTopFilesQueriesNow($theLimit, $siteName);
    foreach $table (@topPerfTables) { &runQuery("TRUNCATE TABLE $table"); }
}

sub runTopUsersQueriesPast() {
    my ($theKeyword, $theLimit, $siteName) = @_;

    print "# updating topPerf USERS tables for $theKeyword\n";

    $destinationTable = "${siteName}_topPerfUsersPast";

    # past jobs
    &runQuery("INSERT INTO jj
         SELECT userId, 
                COUNT(DISTINCT pId, clientHId) AS n
           FROM ${siteName}_closedSessions_Last$theKeyword
       GROUP BY userId");

    # past files - through opened & closed sessions
    &runQuery("INSERT INTO ff           
        SELECT tmp.userId, 
               COUNT(tmp.pathId),
               SUM(tmp.size)/(1024*1024)
          FROM ( SELECT DISTINCT oc.userId, oc.pathId, oc.size
                   FROM ( SELECT userId, pathId, size 
                           FROM  ${siteName}_openedSessions os,
                                 ${siteName}_closedFiles_Last$theKeyword cf,
                                 paths p
                          WHERE  os.id = cf.sessionId     AND
                                 cf.pathId = p.id 
                      UNION ALL
                         SELECT  userId, pathId, size 
                           FROM  ${siteName}_closedSessions_Last$theKeyword cs,
                                 ${siteName}_closedFiles_Last$theKeyword cf,
                                 paths p
                          WHERE  cs.id = cf.sessionId    AND 
                                 cf.pathId = p.id 
                         )   AS  oc
                )   AS tmp
       GROUP BY tmp.userId");

    # past volume - through opened & closed sessions
    &runQuery("INSERT INTO vv
        SELECT oc.userId, 
               SUM(oc.bytesR)/(1024*1024)
          FROM ( SELECT  userId, bytesR
                   FROM  ${siteName}_openedSessions os,
                         ${siteName}_closedFiles_Last$theKeyword cf
                  WHERE  os.id = cf.sessionId
              UNION ALL
                 SELECT  userId, bytesR
                   FROM  ${siteName}_closedSessions_Last$theKeyword cs,
                         ${siteName}_closedFiles_Last$theKeyword cf
                  WHERE  cs.id = cf.sessionId
               )     AS  oc
      GROUP BY oc.userId");

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
                 FROM xx 
                      LEFT OUTER JOIN jj ON xx.theId = jj.theId
                      LEFT OUTER JOIN ff ON xx.theId = ff.theId
                      LEFT OUTER JOIN vv ON xx.theId = vv.theId");
}

sub runTopFilesQueriesPast() {
    my ($theKeyword, $theLimit, $siteName) = @_;

    print "# updating topPerf FILES tables for $theKeyword\n";

    $destinationTable = "${siteName}_topPerfFilesPast";

    # past jobs
    &runQuery("INSERT INTO jj
            SELECT pathId, 
                   COUNT(DISTINCT pId, clientHId) AS n
              FROM ${siteName}_closedSessions_Last$theKeyword cs,
                   ${siteName}_closedFiles_Last$theKeyword cf
             WHERE cs.id = cf.sessionId
          GROUP BY pathId");

    # past volume 
    &runQuery("INSERT INTO vv
        SELECT pathId, 
               SUM(bytesR)/(1024*1024)
          FROM ${siteName}_closedFiles_Last$theKeyword
      GROUP BY pathId");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM vv ORDER BY n DESC LIMIT $theLimit");

    ## delete old data
    &runQuery("DELETE FROM $destinationTable WHERE timePeriod LIKE \"$theKeyword\"");

    ## and finally insert the new data
        &runQuery("INSERT INTO $destinationTable
            SELECT xx.theId, 
                   IFNULL(jj.n, 0) AS jobs,
                   IFNULL(vv.n, 0) AS vol, 
                   \"$theKeyword\"
            FROM   xx 
                   LEFT OUTER JOIN jj ON xx.theId = jj.theId
                   LEFT OUTER JOIN vv ON xx.theId = vv.theId");
}

sub runTopUsersQueriesNow() {
    my ($theLimit, $siteName) = @_;

    print "# updating topPerf USERS tables for NOW\n";

    $destinationTable = "${siteName}_topPerfUsersNow";
    $pastTable        = "${siteName}_topPerfUsersPast";

    # now jobs
    &runQuery("INSERT INTO jj
          SELECT  userId, COUNT(DISTINCT pId, clientHId ) AS n
            FROM  ${siteName}_openedSessions
        GROUP BY  userId");

    # now files
    &runQuery ("INSERT INTO ff 
          SELECT  tmp.userId, 
                  COUNT(tmp.pathId) AS n,
                  SUM(tmp.size)/(1024*1024) AS s
            FROM  (SELECT DISTINCT userId, pathId, size
                     FROM  ${siteName}_openedSessions os,
                           ${siteName}_openedFiles of,
                           paths p
                    WHERE  os.id = of.sessionId     AND
                           of.pathId = p.id
                  )    AS  tmp
        GROUP BY  tmp.userId");

    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM $pastTable");
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY n DESC LIMIT $theLimit");
    &runQuery("REPLACE INTO xx SELECT theId FROM ff ORDER BY s DESC LIMIT $theLimit");

    &runQuery("TRUNCATE TABLE $destinationTable");

    ## and finally insert the new data
    &runQuery("INSERT INTO $destinationTable
          SELECT  DISTINCT xx.theId,
                  IFNULL(jj.n, 0) AS jobs,
                  IFNULL(ff.n, 0) AS files, 
                  IFNULL(ff.s, 0) AS fSize
            FROM  xx 
                  LEFT OUTER JOIN jj ON xx.theId = jj.theId
                  LEFT OUTER JOIN ff ON xx.theId = ff.theId");
}

sub runTopFilesQueriesNow() {
    my ($theLimit, $siteName) = @_;

    print "# updating topPerf FILES tables for NOW\n";

    $destinationTable = "${siteName}_topPerfFilesNow";
    $pastTable        = "${siteName}_topPerfFilesPast";

    # now jobs
    &runQuery("INSERT INTO jj
          SELECT pathId, COUNT(DISTINCT pId, clientHId ) AS n
           FROM  ${siteName}_openedSessions os,
                 ${siteName}_openedFiles of
          WHERE  os.id = of.sessionId
       GROUP BY  pathId");


    ##### now find all names for top X for each sorting 
    &runQuery("REPLACE INTO xx SELECT theId FROM $pastTable");
    &runQuery("REPLACE INTO xx SELECT theId FROM jj ORDER BY n DESC LIMIT $theLimit");

    &runQuery("TRUNCATE TABLE $destinationTable");

    ## and finally insert the new data
    &runQuery("INSERT INTO $destinationTable
          SELECT  DISTINCT xx.theId,
                  IFNULL(jj.n, 0) AS jobs
            FROM  xx 
                  LEFT OUTER JOIN jj ON xx.theId = jj.theId");
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
        WHERE  cs.id = cf.sessionId   AND 
               cf.pathId = p.id
        GROUP BY $idInPathTable");

    # past files
    &runQuery("INSERT INTO ff 
        SELECT tmp.$idInPathTable,
               COUNT(tmp.pathId),
               SUM(tmp.size)/(1024*1024)
          FROM ( SELECT DISTINCT $idInPathTable, pathId, size
                   FROM ${siteName}_closedFiles_Last$theKeyword cf,
                        paths p
                  WHERE cf.pathId = p.id
               )     AS tmp
      GROUP BY tmp.$idInPathTable");


    # past users
    &runQuery("REPLACE INTO uu
       SELECT  $idInPathTable, 
               COUNT(DISTINCT userId) AS n
         FROM  ${siteName}_closedSessions_Last$theKeyword cs,
               ${siteName}_closedFiles_Last$theKeyword cf,
               paths p
        WHERE  cs.id = cf.sessionId   AND
               cf.pathId = p.id
     GROUP BY  $idInPathTable");

    # past volume - through opened & closed sessions
    &runQuery("INSERT INTO vv
         SELECT $idInPathTable, 
                SUM(bytesR/(1024*1024))
           FROM ${siteName}_closedFiles_Last$theKeyword cf,
                paths p
          WHERE cf.pathId = p.id
       GROUP BY $idInPathTable");

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
          FROM ( SELECT  DISTINCT $idInPathTable, pathId, size
                   FROM  ${siteName}_openedFiles of,
                         paths p
                  WHERE  of.pathId = p.id
               )     AS  tmp
      GROUP BY tmp.$idInPathTable");

    # now users
    &runQuery("REPLACE INTO uu 
        SELECT $idInPathTable,
               COUNT(DISTINCT userId) AS n
          FROM ${siteName}_openedSessions os,
               ${siteName}_openedFiles of,
               paths p
         WHERE os.id = of.sessionId
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
