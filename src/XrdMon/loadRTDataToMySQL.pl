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

$initFlag = 1
 

#start an infinite loop
while ( 1 ) {
    doLoading();
    sleep($updInt);
}


sub doLoading {
    my $ts = timestamp();

    # connect to the database
    print "\n$ts Connecting to database...\n";
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName",$mySQLUser) ) {
	print "Error while connecting to database. $DBI::errstr\n";
	return;
    }

    # do initilization in the first connection
    if ( $initFlag ) { doInit(); }

    # lock the file
    unless ( $lockF = lockTheFile($inFName) ) {
	$dbh->disconnect();
	return;
    }
    # set the load time.
    $loadT = timestamp();

     # open the input file for reading
    unless ( open $inF, "< $inFName" ) {
	print "Can't open file $inFName for reading\n";
	unlockTheFile($lockF);
	$dbh->disconnect();
	return;
    }
    # read the file, load the data, close the file
    print "Loading...\n";
    $nr = 1;
    while ( $_ = <$inF> ) {
	#print "processing $_";
	chop;
	if ( $_ =~ m/^u/ ) { loadOpenSession($_);  }
	if ( $_ =~ m/^d/ ) { loadCloseSession($_); }
	if ( $_ =~ m/^o/ ) { loadOpenFile($_);     }
	if ( $_ =~ m/^c/ ) { loadCloseFile($_);    }
	if ( $nr % 10000 == 0 ) {
            $ts = timestamp();
	    print "$ts $nr\n";
	}
	$nr += 1;
    }

    # $nMin, $nHour and $nDay start from 0
    loadStatsLastHour();
    if ( $nMin % 60 == 59 ) {
        loadStatsLastDay();
        if ( $nHour % 24 == 23 ) {
            loadStatsLastMonth();
            $nDay += 1;
            if ( (localtime)[3] == 1 ) { loadStatsAllMonths(); }
        }
        $nHour += 1;
    }
    $nMin += 1


                                                                                                                                                      

    close $inF;
    # make a backup, remove the input file
    my $backupFName = "$inFName.backup";
    `touch $backupFName; cat $inFName >> $backupFName; rm $inFName`;
    # unlock the lock file, and disconnect from db
    unlockTheFile($lockF);
    $dbh->disconnect();
    $ts = timestamp();
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

sub timestamp() {
    my $sec  = (localtime)[0];
    my $min  = (localtime)[1];
    my $hour = (localtime)[2];
    return sprintf("%02d:%02d:%02d", $hour, $min, $sec);
}

sub loadOpenSession() {
    my ($line) = @_;

    ($u, $sessionId, $user, $pid, $clientHost, $srvHost) = split('\t', $line);
    #print "u=$u, id=$id, user=$user, pid=$pid, ch=$clientHost, sh=$srvHost\n";

    my $userId       = findOrInsertUserId($user);
    my $clientHostId = findOrInsertHostId($clientHost);
    my $serverHostId = findOrInsertHostId($srvHost);

    #print "uid=$userId, chid=$clientHostId, shd=$serverHostId\n";
    runQuery("INSERT INTO rtOpenedSessions (id, userId, pId, clientHId, serverHId) VALUES ($sessionId, $userId, $pid, $clientHostId, $serverHostId)");
}


sub loadCloseSession() {
    my ($line) = @_;

    ($d, $sessionId, $sec, $timestamp) = split('\t', $line);
    #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";




    # find if there is corresponding open session, if not don't bother
    my ($userId, $pId, $clientHId, $serverHId) = 
	runQueryWithRet("SELECT userId, pId, clientHId, serverHId FROM rtOpenedSessions WHERE id = $sessionId");
    if ( $pId < 1 ) {
	return;
    }
    #print "received decent data for sId $sessionId: uid=$userId, pid = $pId, cId=$clientHId, sId=$serverHId\n";

    # remove it from the open session table
    runQuery("DELETE FROM rtOpenedSessions WHERE id = $sessionId;");

    # and insert into the closed
    runQuery("INSERT INTO rtClosedSessions (id, userId, pId, clientHId, serverHId, duration, disconnectT) VALUES ($sessionId, $userId, $pId, $clientHId, $serverHId, $sec, \"$timestamp\");");
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

    runQuery("INSERT INTO rtOpenedFiles (id, sessionId, pathId, openT) VALUES ($fileId, $sessionId, $pathId, \"$openTime\")");
}

sub loadCloseFile() {
    my ($line) = @_;

    ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t', $line);
    #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

    # find if there is corresponding open file, if not don't bother
    my ($sessionId, $pathId, $openT) = 
	runQueryWithRet("SELECT sessionId, pathId, openT FROM rtOpenedFiles WHERE id = $fileId");
    if ( ! $sessionId ) {
	return;
    }

    # remove it from the open files table
    runQuery("DELETE FROM rtOpenedFiles WHERE id = $fileId;");

    # and insert into the closed
    runQuery("INSERT INTO rtClosedFiles (id, sessionId, openT, closeT, pathId, bytesR, bytesW) VALUES ($fileId, $sessionId, \"$openT\", \"$closeT\", $pathId, $bytesR, $bytesW);");
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost) = @_;

    my $userId       = findOrInsertUserId($user);
    my $clientHostId = findOrInsertHostId($clientHost);
    my $serverHostId = findOrInsertHostId($srvHost);

    return runQueryWithRet("SELECT id FROM rtOpenedSessions WHERE userId=$userId AND pId=$pid AND clientHId=$clientHostId AND serverHId=$serverHostId;");
}


sub findOrInsertUserId() {
    my ($userName) = @_;

    my $userId = $userIds{$userName};
    if ( $userId ) {
        return $userId;
    }
    $userId = runQueryWithRet("SELECT id FROM users WHERE userName = \"$userName\"");
    if ( $userId ) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting...\n";
	runQuery("INSERT INTO users (userName) VALUES (\"$userName\");");

	$userId = runQueryWithRet("SELECT LAST_INSERT_ID();");
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
    $hostId = runQueryWithRet("SELECT id FROM hosts WHERE hostName = \"$hostName\"");
    if ( $hostId ) {
	#print "Will reuse hostId $clientHostId for $hostName\n";
    } else {
	#print "$hostName not in mysql yet, inserting...\n";
	runQuery("INSERT INTO hosts (hostName) VALUES (\"$hostName\");");

	$hostId = runQueryWithRet("SELECT LAST_INSERT_ID();");
    }
    $hostIds{$hostName} = $hostId;
    return $hostId;
}

sub findOrInsertPathId() {
    my ($path) = @_;
                                                                                                      
    my $pathId = $pathIds{$path};
    if ( $pathId ) {
        print "from cache: $pathId for $path\n";
        return $pathId;
    }
    my($pathId, $typeId, $skimId) =
        runQueryWithRet("SELECT id, typeId, skimId FROM paths WHERE path = \"$path\"");

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
            $typeId = runQueryWithRet("SELECT id FROM fileTypes WHERE name = \"$typeName\"");
        }
        if ( ! $typeId ) {
            runQuery("INSERT INTO fileTypes(name) VALUES(\"$typeName\")");
            $typeId = runQueryWithRet("SELECT LAST_INSERT_ID();");
        }
        # if it is skim, deal with the skim type, if not, 0 would do
        if ( $typeName =~ /skims/ ) {
            # find if the skim name has already id, reuse if it does
            $skimId = $skimNames{$skimName};
            if ( ! $skimId ) {
                $skimId = runQueryWithRet("SELECT id FROM skimNames WHERE name = \"$skimName\"");
            }
            if ( ! $skimId ) {
                runQuery("INSERT INTO skimNames(name) VALUES(\"$skimName\") ");
                $skimId = runQueryWithRet("SELECT LAST_INSERT_ID();");
            }
        }
        runQuery("INSERT INTO paths (path, typeId, skimId) VALUES (\"$path\", $typeId, $skimId);");
        $pathId = runQueryWithRet("SELECT LAST_INSERT_ID();");
                                                                                                      
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
    my ($sql) = @_;
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
    my $lastTime = runQueryWithRet("SELECT MAX(date) FROM statsLastHour");
    if ( $lastTime ) {
        ($nMin, $lastNoJobs, $lastNoUsers, $lastNoUniqueF, $lastNoNonUniqueF) 
            = runQueryWithRet("SELECT seqNo, noJobs, noUsers, noUniqueF, noNonUniqueF
                                 FROM statsLastHour 
                                WHERE date = \"$lastTime\"");
        $nMin += 1;
    } else { 
        $nMin = $lastNoJobs = $lastNoUsers = $lastNoUniqueF = $lastNoNonUniqueF = 0;
    }

    $lastTime = runQueryWithRet("SELECT MAX(date) FROM statsLastDay");
    if ( $lastTime ) {
        $nHour = runQueryWithRet("SELECT seqNo FROM statsLastDay WHERE date = \"$lastTime\"");
        $nHour += 1;
    } else { 
        $nHour = 0;
    }

    $lastTime = runQueryWithRet("SELECT MAX(date) FROM statsLastMonth");
    if ( $lastTime ) {
        $nDay = runQueryWithRet("SELECT seqNo FROM statsLastMonth WHERE date = \"$lastTime\"");
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
    my $seqNo = $nMin % 60;
    runQuery("DELETE FROM statsLastHour WHERE seqNo = $seqNo");
    my ($noJobs, $noUsers) = runQueryWithRet("SELECT COUNT(*), COUNT(DISTINCT userId) 
                                                FROM rtOpenedSessions");

    my ($noUniqueF, $noNonUniqueF) = runQueryWithRet("SELECT COUNT(DISTINCT pathId), COUNT(*) 
                                                        FROM rtOpenedFiles");
    runQuery("INSERT INTO statsLastHour 
                          (seqNo, date, noJobs, noUsers, noUniqueF, noNonUniqueF) 
                   VALUES ($seqNo, \"$loadT\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF)");

    runQuery("DELETE FROM rtChanges");
    my $deltaJobs = $noJobs - $lastNoJobs; 
    my $jobs_p = $lastNoJobs > 0 ? 100 * $deltaJobs / $lastNoJobs : 9999.9;
    my $deltaUsers = $noUsers - $lastNoUsers
    my $users_p = $lastNoUsers > 0 ? 100 * $deltaUsers / $lastNoUsers : 9999.9;
    my $deltaUniqueF = $noUniqueF - $lastNoUniqueF
    my $uniqueF_p = $lastNoUniqueF > 0 ? 100 * $deltaUniqueF / $lastNoUniqueF : 9999.9;
    my $deltaNonUniqueF = $noNonUniqueF - $lastNoNonUniqueF
    my $nonUniqueF_p = $lastNoNonUniqueF > 0 ? 100 * $deltaNonUniqueF / $lastNoNonUniqueF : 9999.9;
    runQuery("INSERT INTO rtChanges 
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
    runQuery("DELETE FROM statsLastDay WHERE seqNo = $seqNo");

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF) 
      = runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastHour");

    runQuery("INSERT INTO statsLastDay 
                          (seqNo, date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                   VALUES ($seqNo, \"$loadT\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
                           $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                           $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");

}

sub loadStatsLastMonth() {
    my $seqNo = $nDay % 31;
    runQuery("DELETE FROM statsLastMonth WHERE seqNo = $seqNo");

    my ($noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
        $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
        $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)
      = runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastHour");

    runQuery("INSERT INTO statsLastMonth 
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
      = runQueryWithRet("SELECT AVG(noJobs), AVG(noUsers), AVG(noUniqueF), AVG(noNonUniqueF), 
                                MIN(noJobs), MIN(noUsers), MIN(noUniqueF), MIN(noNonUniqueF), 
                                MAX(noJobs), MAX(noUsers), MAX(noUniqueF), MAX(noNonUniqueF)  
                           FROM statsLastMonth WHERE MONTH(date) = \"$lastMonth\""); 

    runQuery("INSERT INTO statsAllMonths 
                          (date, noJobs, noUsers, noUniqueF, noNonUniqueF, 
                           minJobs, minUsers, minUniqueF, minNonUniqueF, 
                           maxJobs, maxUsers, maxUniqueF, maxNonUniqueF) 
                    VALUES (\"$loadT\", $noJobs, $noUsers, $noUniqueF, $noNonUniqueF, 
                            $minJobs, $minUsers, $minUniqueF, $minNonUniqueF, 
                            $maxJobs, $maxUsers, $maxUniqueF, $maxNonUniqueF)");
}




