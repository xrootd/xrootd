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



$inFName = $ARGV[0];
open inF, "< $inFName" or die "Can't open file $inFName for reading\n";
while ( $_ = <inF> ) {
    chop;
    if ( $_ =~ m/^u/ ) { loadOpenSession($_);  }
    if ( $_ =~ m/^d/ ) { loadCloseSession($_); }
    if ( $_ =~ m/^o/ ) { loadOpenFile($_);     }
    if ( $_ =~ m/^c/ ) { loadCloseFile($_);    }
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

    my $sql = "INSERT INTO openedSessions (userId, pId, clientHId, serverHId) VALUES ($userId, $pid, $clientHostId, $serverHostId)";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";

    $sql = "INSERT INTO hosts (hostName) VALUES (\"$clientHost\")";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
}


sub loadCloseSession() {
    my ($line) = @_;

    ($d, $sessionId, $sec, $timestamp) = split('\t', $line);
    #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";

    # find if there is corresponding open session, if not don't bother
    my $sql = "SELECT userId, pId, clientHId, serverHId FROM openedSessions WHERE id = $sessionId";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";

    my ($userId, $pId, $clientHId, $serverHId) = $sth->fetchrow_array;
    if ( $pId < 1 ) {
	return;
    }
    #print "received decend data for sId $sessionId: uid=$userId, pid = $pId, cId=$clientHId, sId=$serverHId\n";

    # remove it from the open session table
    my $sql = "DELETE FROM openedSessions WHERE id = $id;";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";

    # and insert into the closed
    my $sql = "INSERT INTO closedSessions (sessionId, userId, pId, clientHId, serverHId, duration, disconnectT) VALUES ($sessionId, $userId, $pId, $clientHId, $serverHId, $sec, \"$timestamp\");";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
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

    my $sql = "INSERT INTO openedFiles (sessionId, openT, filePath) VALUES ($sessionId, \"$openTime\", \"$path\")";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
}

sub loadCloseFile() {
    my ($line) = @_;

    ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t', $line);
    #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

    # find if there is corresponding open file, if not don't bother
    my $sql = "SELECT sessionId, openT, filePath FROM openedFiles WHERE id = $fileId";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    my ($sessionId, $openT, $filePath) = $sth->fetchrow_array;
    if ( ! $sessionId ) {
	return;
    }

    # remove it from the open files table
    my $sql = "DELETE FROM openedFiles WHERE id = $fileId;";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";

    # and insert into the closed
    my $sql = "INSERT INTO closedFiles (sessionId, openT, closeT, bytesR, bytesW, filePath) VALUES ($sessionId, \"$openT\", \"$closeT\", $bytesR, $bytesW, \"$filePath\");";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost) = @_;

    my $userId       = findOrInsertUserId($user);
    my $clientHostId = findOrInsertHostId($clientHost);
    my $serverHostId = findOrInsertHostId($srvHost);

    my $sql = "SELECT id FROM openedSessions WHERE userId=$userId AND clientHId=$clientHostId AND serverHId=$serverHostId;";

#    my $sql = "SELECT openedSessions.id FROM openedSessions, users, hosts WHERE userName=\"$user\" AND users.id=openedSessions.userId AND pid=$pid AND hostName=\"$clientHost\" AND hosts.id=openedSessions.clientHId AND hostName=\"$srvHost\" AND hosts.id=openedSessions.serverHId;";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    my $sessionId = $sth->fetchrow_array;
    return $sessionId;
}


sub findOrInsertUserId() {
    my ($userName) = @_;

    my $sql = "SELECT id FROM users WHERE userName = \"$userName\"";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    my $userId = $sth->fetchrow_array;
    if ( $ userId ) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting...\n";
	my $sql = "INSERT INTO users (userName) VALUES (\"$userName\");";
	my $sth = $dbh->prepare($sql);
	$sth->execute || die "Failed to exec \"$sql\"";

	my $sql = "SELECT LAST_INSERT_ID();";
	my $sth = $dbh->prepare($sql);
	$sth->execute || die "Failed to exec \"$sql\"";
	$userId = $sth->fetchrow_array;
    }
    return $userId;
}

sub findOrInsertHostId() {
    my ($hostName) = @_;

    my $sql = "SELECT id FROM hosts WHERE hostName = \"$hostName\"";
    my $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    my $hostId = $sth->fetchrow_array;
    if ( $hostId ) {
	#print "Will reuse hostId $clientHostId for $hostName\n";
    } else {
	#print "$hostName not in mysql yet, inserting...\n";
	my $sql = "INSERT INTO hosts (hostName) VALUES (\"$hostName\");";
	my $sth = $dbh->prepare($sql);
	$sth->execute || die "Failed to exec \"$sql\"";

	my $sql = "SELECT LAST_INSERT_ID();";
	my $sth = $dbh->prepare($sql);
	$sth->execute || die "Failed to exec \"$sql\"";
	$hostId = $sth->fetchrow_array;
    }
    return $hostId;
}

sub printHelp() {

  print "loadRTDataToMySQL\n";
  print "    [-donotload]\n";
  print "\n";
  print " -donotload    Turns off loading data to MySQL. MySQL script is produced.\n";
  print "\n";
}


