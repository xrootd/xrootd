#!/usr/local/bin/perl

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
    # lock the file
    unless ( $lockF = lockTheFile($inFName) ) {
	$dbh->disconnect();
	return;
    }
    # open the input file for reading
    unless ( open $inF, "< $inFName" ) {
	print "Can't open file $inFName for reading\n";
	unlockTheFile($lockF);
	$dbh->disconnect();
	return;
    }
    # read the file, load the data, close the file
    print "Loading...\n";
    $nr = 0;
    while ( $_ = <$inF> ) {
	#print "processing $_";
	chop;
	if ( $_ =~ m/^u/ ) { loadOpenSession($_);  }
	if ( $_ =~ m/^d/ ) { loadCloseSession($_); }
	if ( $_ =~ m/^o/ ) { loadOpenFile($_);     }
	if ( $_ =~ m/^c/ ) { loadCloseFile($_);    }
	$nr += 1;
	if ( $nr % 10001 == 10000 ) {
            $ts = timestamp();
	    print "$ts $nr\n";
	}
    }

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

    ($u, $id, $user, $pid, $clientHost, $srvHost) = split('\t', $line);
    #print "u=$u, id=$id, user=$user, pid=$pid, ch=$clientHost, sh=$srvHost\n";

    my $userId       = findOrInsertUserId($user);
    my $clientHostId = findOrInsertHostId($clientHost);
    my $serverHostId = findOrInsertHostId($srvHost);

    #print "uid=$userId, chid=$clientHostId, shd=$serverHostId\n";
    runQuery("INSERT INTO openedSessions (userId, pId, clientHId, serverHId) VALUES ($userId, $pid, $clientHostId, $serverHostId)");
}


sub loadCloseSession() {
    my ($line) = @_;

    ($d, $sessionId, $sec, $timestamp) = split('\t', $line);
    #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";

    # find if there is corresponding open session, if not don't bother
    my ($userId, $pId, $clientHId, $serverHId) = 
	runQueryWithRet("SELECT userId, pId, clientHId, serverHId FROM openedSessions WHERE id = $sessionId");
    if ( $pId < 1 ) {
	return;
    }
    #print "received decent data for sId $sessionId: uid=$userId, pid = $pId, cId=$clientHId, sId=$serverHId\n";

    # remove it from the open session table
    runQuery("DELETE FROM openedSessions WHERE id = $sessionId;");

    # and insert into the closed
    runQuery("INSERT INTO closedSessions (id, userId, pId, clientHId, serverHId, duration, disconnectT) VALUES ($sessionId, $userId, $pId, $clientHId, $serverHId, $sec, \"$timestamp\");");
}


sub loadOpenFile() {
    my ($line) = @_;

    my ($o, $id, $user, $pid, $clientHost, $path, $openTime, $srvHost) = 
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

    runQuery("INSERT INTO openedFiles (sessionId, pathId, openT) VALUES ($sessionId, $pathId, \"$openTime\")");
}

sub loadCloseFile() {
    my ($line) = @_;

    ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t', $line);
    #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

    # find if there is corresponding open file, if not don't bother
    my ($sessionId, $pathId, $openT) = 
	runQueryWithRet("SELECT sessionId, pathId, openT FROM openedFiles WHERE id = $fileId");
    if ( ! $sessionId ) {
	return;
    }

    # remove it from the open files table
    runQuery("DELETE FROM openedFiles WHERE id = $fileId;");

    # and insert into the closed
    runQuery("INSERT INTO closedFiles (sessionId, openT, closeT, pathId, bytesR, bytesW) VALUES ($sessionId, \"$openT\", \"$closeT\", $pathId, $bytesR, $bytesW);");
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost) = @_;

    my $userId       = findOrInsertUserId($user);
    my $clientHostId = findOrInsertHostId($clientHost);
    my $serverHostId = findOrInsertHostId($srvHost);

    return runQueryWithRet("SELECT id FROM openedSessions WHERE userId=$userId AND clientHId=$clientHostId AND serverHId=$serverHostId;");
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
        $userIds{$userName} = $userId;
    }
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
	$hostIds{$hostName} = $hostId;
    }
    return $hostId;
}


sub findOrInsertPathId() {
    my ($path) = @_;

    my $pathId = $pathIds{$path};
    if ( $pathId ) {
        return $pathId;
    }
    $pathId = runQueryWithRet("SELECT id FROM paths WHERE path = \"$path\"");
    if ( $pathId ) {
	#print "Will reuse pathId for $path\n";
    } else {
	#print "$path not in mysql yet, inserting...\n";
	runQuery("INSERT INTO paths (path) VALUES (\"$path\");");

	$pathId = runQueryWithRet("SELECT LAST_INSERT_ID();");
	$pathIds{$path} = $pathId;
    }
    return $pathId;
}

sub runQueryWithRet() {
    my ($sql) = @_;
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
    #print "$sql\n";
    return $sth->fetchrow_array;
}

sub runQuery() {
    my ($sql) = @_;
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
    #print "$sql\n";
}


sub printHelp() {

  print "loadRTDataToMySQL\n";
  print "    [-donotload]\n";
  print "\n";
  print " -donotload    Turns off loading data to MySQL. MySQL script is produced.\n";
  print "\n";
}


