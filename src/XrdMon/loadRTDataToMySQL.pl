#!/usr/bin/perl

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


if ( @ARGV ne 1 ) {
    print "Expected arg: <inputFileName>\n";
    exit;
}


$dbh = DBI->connect('dbi:mysql:rtxrdmon',"becla");


$nr = 0;
$inFName = $ARGV[0];
open inF, "< $inFName" or die "Can't open file $inFName for reading\n";
while ( $_ = <inF> ) {
    chop;
    if ( $_ =~ m/^u/ ) { loadOpenSession($_);  }
    if ( $_ =~ m/^d/ ) { loadCloseSession($_); }
    if ( $_ =~ m/^o/ ) { loadOpenFile($_);     }
    if ( $_ =~ m/^c/ ) { loadCloseFile($_);    }
    $nr += 1;
    if ( $nr % 1001 == 1000 ) {
($Second, $Minute, $Hour, $Day, $Month, $Year, $WeekDay, $DayOfYear, $IsDST) = localtime(time) ; 
	print "$Hour:$Minute:$Second $nr\n";
    }
}
close inF;

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
    runQuery("DELETE FROM openedSessions WHERE id = $id;");

    # and insert into the closed
    runQuery("INSERT INTO closedSessions (id, userId, pId, clientHId, serverHId, duration, disconnectT) VALUES ($sessionId, $userId, $pId, $clientHId, $serverHId, $sec, \"$timestamp\");");
}


sub loadOpenFile() {
    my ($line) = @_;

    my ($o, $id, $user, $pid, $clientHost, $path, $openTime, $srvHost) = split('\t', $line);
    #print ("\no=$o, id=$id, user=$user, pid=$pid, ch=$clientHost, p=$path, time=$openTime, srvh=$srvHost\n");

    my $sessionId = findSessionId($user, $pid, $clientHost, $srvHost);
    if ( ! $sessionId ) {
	#print "session id not found for $user $pid $clientHost $srvHost\n";
	return; # error: no corresponding session id
    }

    runQuery("INSERT INTO openedFiles (sessionId, openT, filePath) VALUES ($sessionId, \"$openTime\", \"$path\")");
}

sub loadCloseFile() {
    my ($line) = @_;

    ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t', $line);
    #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

    # find if there is corresponding open file, if not don't bother
    my ($sessionId, $openT, $filePath) = 
	runQueryWithRet("SELECT id, openT, filePath FROM openedFiles WHERE id = $fileId");
    if ( ! $sessionId ) {
	return;
    }

    # remove it from the open files table
    runQuery("DELETE FROM openedFiles WHERE id = $fileId;");

    # and insert into the closed
    runQuery("INSERT INTO closedFiles (id, openT, closeT, bytesR, bytesW, filePath) VALUES ($sessionId, \"$openT\", \"$closeT\", $bytesR, $bytesW, \"$filePath\");");
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
    if ( $ userId ) {
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

sub runQuery() {
    my ($sql) = @_;
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    #print "\n$sql\n";
}

sub runQueryWithRet() {
    my ($sql) = @_;
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    #print "\n$sql\n";
    return $sth->fetchrow_array;
}


sub printHelp() {

  print "loadRTDataToMySQL\n";
  print "    [-donotload]\n";
  print "\n";
  print " -donotload    Turns off loading data to MySQL. MySQL script is produced.\n";
  print "\n";
}


