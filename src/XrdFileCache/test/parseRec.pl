#!/usr/bin/env perl

use strict;
use warnings;



my $sbPrefCh   = 0;
my $sbPref   = 0;
my $sbDisk   = 0;
my $sbError   = 0;


my $argc = 0;
foreach my $arg (@ARGV) {


  my $path = $arg; #shift(@ARGV);

  print "-------------------------------$arg \n";

  open( my $fh, '<', $path)
      or die "Could not open $path!";



  my $reHits = '/(.*)Detach: NumHits\[(\d+)\] NumHitsPrefetch\[(\d+)\] NumHitsDisk\[(\d+)\] NumMissed\[(\d+)\] (.*)';

# bPrefCh[1825207267] bPref[1828703832] bDisk[0] bError[0] /store/
  my $reBytes = 'bPrefCh\[(\d+)\] bPref\[(\d+)\] bDisk\[(\d+)\] bError\[(\d+)\] (.*)';
#my $reBytes = 'bPrefCh\[(\d+)\]';



  while (my $line = <$fh>){ 
    # print $line;
    chomp $line;

    if($line =~ /$reHits/ ) {
      # file name
      my $path       = $1;


      # hits count;
      my $hits       = $2;
      my $hitsPrefetch       = $3;
      my $hitsDisk       = $4;
      my $missed     = $5;
      # print $line, "\n\n";
      # print "=== ", $2,", ", $3,", ", $4,", ", $5, "==== NHITS ===================\n";
    }
    # bytes

    if($line =~ /$reBytes/ ) { 
      #  print $1, ", ", $2, ", ", $3, ", ", $4, ", ",  $5, "========= BYTES ==========\n";

      my $bPrefCh   = $1;
      my $bPref   = $2;
      my $bDisk   = $3;
      my $bError  = $4;

      $sbPrefCh +=  $bPrefCh;
      $sbPref  +=  $bPref;
      $sbDisk  +=  $bDisk;
      $sbError +=  $bError;

      my $path = $5;
      my $name = $5;
      if ($path =~ /(.*)\/(.*)\.root/) {
        $name = $2;
        $name .= ".root";
      }
      printf( "bPrefCh %-12s  bPref %-12s | bDisk =  %-12s bError = %-12s name = (%12s)\n", $bPrefCh, $bPref, $bDisk, $bError, $name); 

    }

  } # loop file

} # loop files

my $bHit = $sbPrefCh +  $sbDisk;

printf( "sbPrefCh %-12s  sbPref %-12s | sbDisk =  %-12s sbError = %-12s \n", $sbPrefCh, $sbPref, $sbDisk, $sbError); 
printf("bHit %s bMiss %s, ratio %.2f \n", $bHit, $sbPref, $bHit/ ($bHit + $sbPref) )


