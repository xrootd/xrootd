#!/usr/local/bin/perl5.8

#  $Id$

use XrdClientAdmin;
XrdClientAdmin::XrdCA_Initialize("root://kanolb-a/dummy");

$par = "/store/PR/R14/AllEvents/0004/35/14.2.0b/AllEvents_00043511_14.2.0bV00.02E.root";
$ans = XrdClientAdmin::XrdCA_ExistFiles($par);
print "\nThe answer of XTNetAdmin_ExistFiles($par) is: \"$ans\" \n\n\n";

$par = "/store/PR";
$ans = XrdClientAdmin::XrdCA_ExistDirs($par);
print "\nThe answer of XTNetAdmin_ExistDirs($par) is: \"$ans\" \n\n\n";

$par = "/store\n/store/PR/R14/AllEvents/0004/35/14.2.0b/AllEvents_00043511_14.2.0bV00.02E.root\n/tmp";
$ans = XrdClientAdmin::XrdCA_ExistFiles($par);
print "\nThe answer of XTNetAdmin_ExistFiles($par) is: \"$ans\" \n\n\n";

$par = "/data/babar/kanga\n/etc\n/mydir\n/store/PR";
$ans = XrdClientAdmin::XrdCA_ExistDirs($par);
print "\nThe answer of XTNetAdmin_ExistDirs($par) is: \"$ans\" \n\n\n";

$par = "/store/PR/R14/AllEvents/0004/35/14.2.0b/AllEvents_00043511_14.2.0bV00.02E.root";
$ans = XrdClientAdmin::XrdCA_IsFileOnline("$par");
print "\nThe answer of XTNetAdmin_IsFileOnline($par) is: \"$ans\" \n\n\n";

XrdClientAdmin::XrdCA_Terminate();

