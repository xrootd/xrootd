#******************************************************************************
#   $Id$
#*                                                                            *
#*                       X r d O l b N o t i f y . p m                        *
#*                                                                            *
# (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University   *
#                          All Rights Reserved                                *
# Produced by Andrew Hanushevsky for Stanford University under contract       *
#            DE-AC03-76-SFO0515 with the Department of Energy                 *
#******************************************************************************
  
#!/usr/local/bin/perl
package  XrdOlbNotify;
require  Exporter;
@ISA   = qw(Exporter);
@EXPORT= qw(setDebug FileGone FileHere SendMsg);
use Socket;
{1}

#******************************************************************************
#*                              F i l e G o n e                               *
#******************************************************************************
 
#Call:   FileGone(paths);

#Input:  @paths   - an array of paths that are no longer on the server.
#
#Processing:
#        The message is sent to the manager OLB's informing them that each file
#        if no longer available on this server.
#
#Output: None.
  
sub FileGone {my(@paths) = @_;
    my($file);

# Send a message for each path in the path list
#
foreach $file (@paths) {&SendMsg("gone $file");}
}

#******************************************************************************
#*                              F i l e H e r e                               *
#******************************************************************************
 
#Call:   FileHere(paths);

#Input:  @paths   - an array of paths that are available on the server.
#
#Processing:
#        The message is sent to the manager OLB's informing them that each file
#        if now available on this server.
#
#Output: None.
  
sub FileHere {my(@paths) = @_;
    my($file);

# Send a message for each path in the path list
#
foreach $file (@paths) {&SendMsg("have $file");}
}

#******************************************************************************
#*                               S e n d M s g                                *
#******************************************************************************
 
#Input:  $msg     - message to be sent
#
#Processing:
#        The message is sent to the olb udp path indicated in the olbd.pid file
#
#Output: 0 - Message sent
#        1 - Message not sent, could not find the pid file or olb not running
#
#Notes:  1. If an absolute path is given, we check whether the <pid> in the
#           file is still alive. If it is not, then no messages are sent.
  
sub SendMsg {my($msg) = @_;

# Allocate a socket if we do not have one
#
  if (!fileno(OLBSOCK) && !socket(OLBSOCK, PF_UNIX, SOCK_DGRAM, 0))
     {print STDERR  "OlbNotify: Unable to create socket; $!\n";
      return 1;
     }

# Get the target if we don't have it
#
  return 1 if (!defined($OLBADDR) || !kill(0, $OLBPID)) && !getConfig();

# Send the message
#
  print STDERR "OlbNotify: Sending message '$msg'\n" if $DEBUG;
  chomp($msg);
  return 0 if send(OLBSOCK, "$msg\n", 0, $OLBADDR);
  print STDERR "OlbNotify: Unable to send to olb $OLBPID; $!\n" if $DEBUG;
  return 0;
}

#******************************************************************************
#*                              s e t D e b u g                               *
#******************************************************************************
 
#Input:  $dbg     - True if debug is to be turned off; false otherwise.
#
#Processing:
#        The global debug flag is set.
#
#Output: Previous debug setting.

sub setDebug {my($dbg) = @_; 
    my($olddbg) = $DEBUG; 
    $DEBUG = $dbg; 
    return $olddbg;
}

#******************************************************************************
#*                     P r i v a t e   F u n c t i o n s                      *
#******************************************************************************
#******************************************************************************
#*                               g e t S o c k                                *
#******************************************************************************
  
sub getSock {my($path);

# Get the path we are to use
#
  return 0 if !($path = getConfig());

# Create the path we are to use
#
  $path = "$path/olbd.notes";
  $OLBADDR = sockaddr_un($path);

# Create a socket
#
  if (!socket(OLBSOCK, PF_UNIX, SOCK_DGRAM, 0))
     {print STDERR  "OlbNotify: Unable to create socket; $!\n";
      return 0;
     }
  return 1;
}

#******************************************************************************
#*                             g e t C o n f i g                              *
#******************************************************************************
 
sub getConfig {my($fn, @phval, $path, $line);

# We will look for the pid file in one of two locations
#
  if (-r '/tmp/olbd.pid') {$fn = '/tmp/olbd.pid';}
     elsif (-r '/var/run/olbd/olbd.pid') {$fn = '/var/run/olbd/olbd.pid';}
        else {print STDERR "OlbNotify: Unable to find olbd pid file\n" if $DEBUG;
              return '';
             }

    if (!open(INFD, $fn))
       {print STDERR "OlbNotify: Unable to open $fn; $!\n" if $DEBUG; return '';}

    @phval = <INFD>;
    close(INFD);
    chomp(@phval);
    $OLBPID = shift(@phval);
    undef($path);
    if (kill(0, $OLBPID))
       {foreach $line (@phval) 
           {($path) = $line =~ m/^&ap=(.*)$/; last if $path;}
        if ($path) {$OLBADDR = sockaddr_un("$path/olbd.notes");}
           else {print STDERR "OlbNotify: Can't find olb admin path\n" if $DEBUG;}
       } else   {print STDERR "OlbNotify: olbd process $OLBPID dead\n" if $DEBUG;}
    return $path;
}
