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

    $userId       = findOrInsertUserId($user);
    $clientHostId = findOrInsertHostId($clientHost);
    $serverHostId = findOrInsertHostId($srvHost);

    $sql = "INSERT INTO openedSessions (userId, pId, clientHId, serverHId) VALUES ($userId, $pid, $clientHostId, $serverHostId)";
    $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";

    $sql = "INSERT INTO hosts (hostName) VALUES (\"$clientHost\")";
}


sub loadCloseSession() {
    my ($line) = @_;

    ($d, $sessionId, $sec, $timestamp) = split('\t', $line);
    #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";

    # find if there is corresponding open session, if not don't bother
    $sql = "SELECT userId, pId, clientHId, serverHId FROM openedSessions WHERE id = $sessionId";
    $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";

    my ($userId, $pId, $clientHId, $serverHId) = $sth->fetchrow_array;
    if ( $pId < 1 ) {
	return;
    }
    #print "received decend data for sId $sessionId: uid=$userId, pid = $pId, cId=$clientHId, sId=$serverHId\n";

    # remove it from the open session table
    $sql = "REMOVE FROM openedSessions WHERE id = $id";

    # and insert into the closed
    $sql = "INSERT INTO closedSessions (sessionId, userId, pId, clientHId, serverHId, duration, disconnectT) VALUES ($sessionId, $userId, $pId, $clientHId, $serverHId, $sec, \"$timestamp\");";
    $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
}


sub loadOpenFile() {
}


sub loadCloseFile() {
}



sub findOrInsertUserId() {
    my ($userName) = @_;

    $sql = "SELECT id FROM users WHERE userName = \"$userName\"";
    $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    if ($userId = $sth->fetchrow_array) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting...\n";
	$sql = "INSERT INTO users (userName) VALUES (\"$userName\");";
	$sth = $dbh->prepare($sql);
	$sth->execute || die "Failed to exec \"$sql\"";

	$sql = "SELECT LAST_INSERT_ID();";
	$sth = $dbh->prepare($sql);
	$sth->execute || die "Failed to exec \"$sql\"";
	$userId = $sth->fetchrow_array;
    }
    return $userId;
}

sub findOrInsertHostId() {
    my ($hostName) = @_;

    $sql = "SELECT id FROM hosts WHERE hostName = \"$hostName\"";
    $sth = $dbh->prepare($sql);
    $sth->execute || die "Failed to exec \"$sql\"";
    if ($hostId = $sth->fetchrow_array) {
	#print "Will reuse hostId $clientHostId for $hostName\n";
    } else {
	#print "$hostName not in mysql yet, inserting...\n";
	$sql = "INSERT INTO hosts (hostName) VALUES (\"$hostName\");";
	$sth = $dbh->prepare($sql);
	$sth->execute || die "Failed to exec \"$sql\"";

	$sql = "SELECT LAST_INSERT_ID();";
	$sth = $dbh->prepare($sql);
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


