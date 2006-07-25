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

if ( @ARGV == 2 ) {
    $configFile = $ARGV[0];
    $action = $ARGV[1];
} else {
    &printUsage('start', 'stop');
    exit;
}

if ( $action eq 'stop' ) {
    &readConfigFile($configFile, 'load', 0);
    $stopFName = "$baseDir/$0";
    substr($stopFName,-2,2,'stop');
    `touch  $stopFName`;
    exit;
} elsif ( $action ne 'start') {
    &printUsage('start', 'stop');
    exit;
}

# Start

&readConfigFile($configFile, 'load', 1);

&initLoad();

foreach $siteName (@siteNames) {
    $firstCall{$siteName} = 1;
}

# and run the main never-ending loop
while ( 1 ) {
    my $sec2Sleep = 60 - (localtime)[0];
    if ( $sec2Sleep < 60 ) {
        print "sleeping $sec2Sleep sec... \n";
        sleep $sec2Sleep;
    } 

    my @localt = localtime(time());
    my $minSec1 = $localt[1]*60 + $localt[0];

    &doLoading();

    if ( -e $stopFName ) {
        &stopLoading();
    }

    # ensure that time before and after loading aren't the same.
    @localt = localtime(time());
    my $minSec2 = $localt[1]*60 + $localt[0];
    if ( $minSec2 == $minSec1 ) {
        sleep(2);
    }
    
}
 
###############################################################################
###############################################################################
###############################################################################
sub backupOneSite() {
    my ($siteName, $loadTime) = @_;
    my $inFN   = $siteInputFiles{$siteName};
    my $siteId = $siteIds{$siteName};
    my $backupInt = $backupInts{$siteName};

    if ( ! -e $inFN || -z $inFN  ) {
        print "File $inFN does not exist or is empty \n";
       	return 0;
    }
    # lock the file
    unless ( $lockF = &lockTheFile($inFN) ) {
        return 0;
    }
    # make a backup of the input file and move it to jrnl directory
    my $nextBackup = &runQueryWithRet("SELECT DATE_ADD(backupTime, INTERVAL $backupInt)
                                         FROM sites
                                        WHERE name = '$siteName'");
    if ( $loadTime ge $nextBackup ) {
         &runQuery("UPDATE sites
                    SET backupTime = '$loadTime'
                    WHERE name = '$siteName'");
                           
         $bkuptime = join "-", split(/ /, $loadTime);
         $backupFile = "$baseDir/$siteName/backup/${siteName}-${bkuptime}-GMT.backup";
         $backupFiles{$siteName} = $backupFile
    } else {
         $backupFile = $backupFiles{$siteName}
    }
    `touch $backupFile; cat $inFN >> $backupFile; mv -f $inFN $baseDir/$siteName/journal/$siteName.ascii; touch $inFN`;

    # unlock the lock file
    unlockTheFile($lockF);
}
sub closeIdleSessions() {
    # closes open sessions with no open files that have been
    # open longer than $maxSessionIdleTime.
    # Assignments:
    # duration = MAX(closeT, connectT) - MIN(openT, connectT)
    # disconnectT = MAX(closeT, connectT)
    # status = I

    my ($siteName, $GMTnow, $loadLastTables ) = @_;

    &printNow("Closing idle sessions... ");

    my $cutOffDate = &runQueryWithRet("SELECT DATE_SUB('$GMTnow', INTERVAL $maxSessionIdleTime)");

    # make temporary table of open sessions with no open files.
    &runQuery("CREATE TEMPORARY TABLE os_no_of LIKE ${siteName}_openedSessions");
    my $noDone = &runQueryRetNum("INSERT INTO os_no_of
                                        SELECT os.*
                                          FROM        ${siteName}_openedSessions os
                                               LEFT JOIN
                                                      ${siteName}_openedFiles of
                                            ON     os.id = of.sessionId
                                         WHERE of.id IS NULL");
    if ( $noDone == 0 ) {
        &runQuery("DROP TABLE IF EXISTS os_no_of");
        return
    }

    &runQuery("CREATE TEMPORARY TABLE cs_no_of LIKE ${siteName}_closedSessions_LastHour");
    # close sessions with closed files
    my $n_cs_cf = 
        &runQueryRetNum("INSERT 
                           INTO cs_no_of
                         SELECT os.id, jobId, userId, pId, clientHId, serverHId,
                                TIMESTAMPDIFF(SECOND, MIN(cf.openT), MAX(cf.closeT)),
                                MAX(cf.closeT) AS maxT,
                                'I'
                           FROM os_no_of os, ${siteName}_closedFiles cf
                          WHERE os.id = cf.sessionId
                       GROUP BY os.id
                         HAVING maxT < '$cutOffDate' ");

    # close sessions with no files
    my $n_cs_no_f =
        &runQueryRetNum("INSERT 
                           INTO cs_no_of
                         SELECT os.id, jobId, userId, pId, clientHId, serverHId, 0, connectT, 'I'
                           FROM     os_no_of os
                                LEFT JOIN
                                    ${siteName}_closedFiles cf
                                ON  os.id = cf.sessionId
                          WHERE cf.id IS NULL   AND
                                os.connectT < '$cutOffDate'     ");

    &updateForClosedSessions("cs_no_of", $siteName, $GMTnow, $loadLastTables);

    &runQuery("DROP TABLE IF EXISTS os_no_of");
    &runQuery("DROP TABLE IF EXISTS cs_no_of");
    print "closed $n_cs_cf sessions with closed and $n_cs_no_f with no files\n"; 
}

sub closeInputFiles() {
    close OFILE;
    close UFILE;
    close DFILE;
    close CFILE;
    close RFILE;
}   
sub closeInteruptedSessions() {
    # closes sessions active before a xrootd restart.
    # Assignments:
    # duration = $restartTime - MIN(file open times, connectT)
    # disconnectT = $restartTime
    # status = R

    my ($hostId, $siteName, $restartTime, $GMTnow, $loadLastTables) = @_;
    &runQuery("CREATE TEMPORARY TABLE cs LIKE ${siteName}_closedSessions");
    my $noDone = &runQueryRetNum("INSERT INTO cs
                                       SELECT os.id, jobId, userId, pId, clientHId, serverHId,
                                              TIMESTAMPDIFF(SECOND,
                                                            LEAST(IFNULL(MIN(of.openT),'3'),
                                                                  IFNULL(MIN(cf.openT),'3'),
                                                                  connectT                  ),
                                                            '$restartTime'                    ) AS duration,
                                              '$restartTime', 'R'
                                         FROM        (${siteName}_openedSessions os
                                              LEFT JOIN
                                                     ${siteName}_openedFiles of
                                                     ON os.id = of.sessionId)
                                              LEFT JOIN
                                                     ${siteName}_closedFiles cf
                                                     ON os.id = cf.sessionId
                                        WHERE serverHId = $hostId
                                     GROUP BY os.id
                                       HAVING duration > 0 ");
    if ( $noDone == 0 ) {
        &runQuery("DROP TABLE IF EXISTS cs");
        return
    }
    &updateForClosedSessions("cs", $siteName, $GMTnow, $loadLastTables);

    &runQuery("DROP TABLE IF EXISTS cs");
    print " closed $noDone interupted sessions \n";
}

sub closeLongSessions() {
    # closes open sessions with associated open files that are
    # open longer than $maxConnectTime.
    # Assignments:
    # disconnectT = MAX(open-file openT, closed-file closeT) 
    # duration = disconnectT - MIN(openT)                  
    # status = L

    my ($siteName, $GMTnow, $loadLastTables) = @_;

    &printNow("Closing long sessions... ");

    my $cutOffDate = &runQueryWithRet("SELECT DATE_SUB('$GMTnow', INTERVAL $maxConnectTime)");
    &runQuery("CREATE TEMPORARY TABLE IF NOT EXISTS cs LIKE ${siteName}_closedSessions_LastHour");
    my $noDone =&runQueryRetNum("
                 INSERT IGNORE INTO cs
                 SELECT os.id, jobId, userId, pId, clientHId, serverHId,
                        TIMESTAMPDIFF(SECOND,
                                      LEAST(IFNULL(MIN(cf.openT),'3'),
                                            MIN(of.openT)           ),
                                      GREATEST(IFNULL(MAX(cf.closeT),'2'),
                                               MAX(of.openT)            )  ),
                        GREATEST(IFNULL(MAX(cf.closeT),'2'),
                                 MAX(of.openT) ),
                        'L'
                   FROM         (${siteName}_openedSessions os,
                                 ${siteName}_openedFiles of)
                        LEFT JOIN
                                 ${siteName}_closedFiles cf
                        ON       os.id = cf.sessionId
                  WHERE 	os.id       = of.sessionId        AND
                        os.connectT < '$cutOffDate'
               GROUP BY os.id");

    if ( $noDone > 0 ) {
        &updateForClosedSessions("cs", $siteName, $GMTnow, $loadLastTables);
    }
    &runQuery("DROP TABLE IF EXISTS cs");
    print "$noDone closed\n";
}


# closes opened files corresponding to closed sessions
sub closeOpenedFiles() {
    my ($siteName, $GMTnow, $loadLastTables) = @_;

    &printNow("Closing open files... ");
    &runQuery("CREATE TEMPORARY TABLE xcf like ${siteName}_closedFiles");

    # give it extra time: sometimes closeFile
    # info may arrive after the closeSession info
    # timeout on xrootd server is ~ 1min, so 10 min should suffice
    # This is the default value of $fileCloseWaitTime
    &runQuery("INSERT IGNORE INTO xcf
                 SELECT of.id,
                        of.sessionId,
                        of.pathId,
                        of.openT,
                        cs.disconnectT,
                        -1,
                        -1
                 FROM   ${siteName}_openedFiles of,
                        ${siteName}_closedSessions cs
                 WHERE  sessionId = cs.id    AND
                        disconnectT < DATE_SUB('$GMTnow', INTERVAL $fileCloseWaitTime)");

    &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles
                           SELECT * 
                             FROM xcf");

    if ( $loadLastTables ) {
        foreach $period ( @periods ) {
            next if ( $period eq "Hour" );
            &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_Last$period
                       SELECT * 
                         FROM xcf 
                        WHERE closeT > DATE_SUB('$GMTnow', INTERVAL 1 $period)");

        }
    } else {
        &runQuery("INSERT IGNORE INTO closedFiles
                   SELECT * 
                    FROM xcf ");
    }
 
    my $noDone =&runQueryRetNum("DELETE FROM  ${siteName}_openedFiles
                                  USING ${siteName}_openedFiles, xcf
                                  WHERE ${siteName}_openedFiles.id = xcf.id");
    &runQuery("DROP TABLE xcf");
    print " $noDone closed\n";
}

sub doLoading {

    # load data for each site
    my $gmts = &gmtimestamp();
    my $nr = 0;
    foreach $siteName (@siteNames) {
        my $jrnlDir = "$baseDir/$siteName/journal";
        next if ( -e "$jrnlDir/inhibitLoading" );
        if ( $firstCall{$siteName} ) {
            &initLoad4OneSite($siteName);
            `touch $jrnlDir/inhibitPrepare`;
            `touch $jrnlDir/loadingActive`;
            &recoverLoad4OneSite($siteName);
            $firstCall{$siteName} = 0;
            `rm -f $jrnlDir/inhibitPrepare`;
            next;
        }
        `touch $jrnlDir/loadingActive`;
        &backupOneSite($siteName, $gmts);
	$nr += &loadOneSite($siteName, $gmts, 1);
    }

    $ts = &timestamp();
    print "$ts All done, processed $nr entries.\n";

}
sub findOrInsertHostId() {
    my ($hostName, $siteName, $hostType) = @_;

    my $hostId = $hostIds{$hostName};
    if ( $hostId ) {
        return $hostId;
    }
    $hostId = &runQueryWithRet("SELECT id
                                FROM ${siteName}_hosts
                                WHERE hostName = '$hostName'");
    if ( $hostId ) {
	#print "Will reuse hostId $clientHostId for $hostName\n";
    } else {
	#print "$hostName not in mysql yet, inserting...\n";
	&runQuery("INSERT INTO ${siteName}_hosts (hostName, hostType)
                   VALUES ('$hostName', $hostType)");

	$hostId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
    }
    $hostIds{$hostName} = $hostId;
    return $hostId;
}
sub findOrInsertPathId() {
    my ($path, $size) = @_;

    my $pathId = $pathIds{$path};
    if ( $pathId ) {
        #print "from cache: $pathId for $path\n";
        return $pathId;
    }

    # Is $path in database?
    my $hashValue = &returnHash("$path");
    $pathId = &runQueryWithRet("SELECT id FROM paths 
                                 WHERE hash = $hashValue AND 
                                       name = '$path'");


    if ( ! $pathId ) {
        #print "$path not in mysql yet, inserting...\n";

        # get file types from path name
        my @fileTypeValues = &getFileTypes("$path");

        # loop over file types to see if they already have ids.
        $n = 0;
        my @typeIdList = ();
        foreach $typeValue ( @fileTypeValues ) {
            if ( $typeValue eq "undef" ) {
                 $typeId = 0;
            } else {
                $tId = $fileTypeOrder[$n];
                $typeId = $fileTypeIds[$tId]{$typeValue};
                if ( ! $typeId ) {
                    &runQuery("INSERT INTO fileType_$tId (name) 
                                    VALUES ('$typeValue')");
                    $typeId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
                    $fileTypeIds[$tId]{$typeValue} = $typeId;
                }
            }
            push @typeIdList , $typeId;
            $n++;
        }
        $typeIdList = join ',' , @typeIdList;
        &runQuery("INSERT INTO paths (name,hash)
                        VALUES ('$path', $hashValue )");
        $pathId = &runQueryWithRet("SELECT LAST_INSERT_ID()");

        &runQuery("INSERT INTO fileInfo (id, size, $fileTypeList)
                        VALUES ($pathId, $size, $typeIdList )");
    }
    $pathIds{$path} = $pathId;
    return $pathId;
}
sub findOrInsertUserId() {
    my ($userName, $siteName) = @_;

    my $userId = $userIds{$userName};
    if ( $userId ) {
        return $userId;
    }
    $userId = &runQueryWithRet("SELECT id 
                                FROM ${siteName}_users
                                WHERE name = '$userName'");
    if ( $userId ) {
	#print "Will reuse user id $userId for $userName\n";
    } else {
	#print "$userName not in mysql yet, inserting... \n";
	&runQuery("INSERT IGNORE INTO ${siteName}_users (name)
                   VALUES ('$userName')");

	$userId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
    }
    $userIds{$userName} = $userId;
    return $userId;
}

sub findSessionId() {
    my($user, $pid, $clientHost, $srvHost, $siteName) = @_;

    my $userId       = &findOrInsertUserId($user, $siteName);
    my $clientHostId = &findOrInsertHostId($clientHost, $siteName, 2);
    my $serverHostId = &findOrInsertHostId($srvHost, $siteName, 1);

    return &runQueryWithRet("SELECT id FROM ${siteName}_openedSessions 
                                       WHERE userId=$userId          AND
                                             pId=$pid                AND
                                             clientHId=$clientHostId AND
                                             serverHId=$serverHostId      ");
}

sub forceClose() {
    my ($siteName, $loadTime, $loadLastTables) = @_;
    if ( $loadTime ge $nextFileClose{$siteName} ) {
        &closeOpenedFiles($siteName, $loadTime, $loadLastTables);
        &runQuery("UPDATE sites
                   SET closeFileT = '$loadTime'
                   WHERE name = '$siteName'");
        $nextFileClose{$siteName} =  &runQueryWithRet("SELECT DATE_ADD('$loadTime', INTERVAL $closeFileInt)");
    }
    
     if ( $loadTime ge $nextIdleSessionClose{$siteName} ) {
        &closeIdleSessions($siteName, $loadTime, $loadLastTables);
        &runQuery("UPDATE sites
                   SET closeIdleSessionT = '$loadTime'
                   WHERE name = '$siteName'");
        $nextIdleSessionClose{$siteName} =  &runQueryWithRet("SELECT DATE_ADD('$loadTime', INTERVAL $closeIdleSessionInt)");
    }    
        
     if ( $loadTime ge $nextLongSessionClose{$siteName} ) {
        &closeLongSessions($siteName, $loadTime, $loadLastTables);
        &runQuery("UPDATE sites
                   SET closeLongSessionT = '$loadTime'
                   WHERE name = '$siteName'");
        $nextLongSessionClose{$siteName} =  &runQueryWithRet("SELECT DATE_ADD('$loadTime', INTERVAL $closeLongSessionInt)");
    }  
}
sub getFileEndTime() {
    my ($siteName, $file) = @_;
    my $tmpFile = "$baseDir/$siteName/journal/tmpFile";
    `tail -500 $file > $tmpFile`;
    unless ( open INPUT, "< $tmpFile" ) {
        print "Can't open file $tmpFile for reading \n";
        exit;
    }
    my $endT = "";
    while ( <INPUT> ) {
        my @line = split('\t');
        if    ( $_ =~ m/^o/ ) { $endT = $line[6];}
        elsif ( $_ =~ m/^d/ ) { $endT = $line[3];}
        elsif ( $_ =~ m/^c/ ) { $endT = $line[4];}
        else {next;}
    }
    close INPUT;
    `rm -f $tmpFile`;
    $endT = &localToGMT($siteName, $endT);
    return($endT);
}
sub getFileStartTime() {
    my ($siteName, $file) = @_;
    unless ( open INPUT, "< $file" ) {
        print "Can't open file $file for reading \n";
        exit;
    }
    my $startT = "";
    while ( <INPUT> ) {
        my @line = split('\t');
        if    ( $_ =~ m/^o/ ) { $startT = $line[6];}
        elsif ( $_ =~ m/^d/ ) { $startT = $line[3];}
        elsif ( $_ =~ m/^c/ ) { $startT = $line[4];}
        else {next;}
        last;
    }
    close INPUT;
    $startT = &localToGMT($siteName, $startT);
    return($startT);
}
sub getFileTypes() {
    $path = $_[0];
    my $typeName = "undef";
    my $skimName = "undef";
    if ( $path ) {
        my @sections = split(/\//, $path);
        $nSections = scalar @sections;
        if ( $nSections > 2 ) {
             $typeName = $sections[2];
             if ( $typeName =~ /skims/ and $nSections > 5 ) {
                  $skimName = $sections[5];
             }
        }
        return ($typeName, $skimName);
    } else {
        return ('dataType','skim');
    }
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
sub initLoad() {
    # do the necessary one-time initialization
    
    $stopFName = "$baseDir/$0";
    substr($stopFName,-2,2,'stop');
    if ( -e $stopFName ) {
       unlink $stopFName;
    }
 
    unless ( $dbh = DBI->connect("dbi:mysql:$dbName;mysql_socket=$mysqlSocket",$mySQLUser) ) {
        print "Error while connecting to database. $DBI::errstr\n";
        exit;
    }

    @periods = ( 'Hour', 'Day', 'Week', 'Month' );
    if ( $yearlyStats ) {push @periods, "Year";}

    $nFileTypes = &runQueryWithRet("SELECT COUNT(*) FROM fileTypes");

    @fileTypes = &getFileTypes("");
    if ( $nFileTypes != scalar @fileTypes ) {
        print "ERROR: Number of types from getFileTypes() does not match the database. \n";
        exit;
    }

    @fileTypeOrder = ();
    my @fileTypeList = ();
    foreach $type ( @fileTypes) {
        my $tId = &runQueryWithRet("SELECT tId
                                      FROM fileTypes
                                     WHERE name = '$type' ");
        if ( ! $tId ) {
            print "ERROR: getFileTypes() returned unrecognized file type $type \n";
            exit;
        }
        push @fileTypeOrder, $tId;
        push @fileTypeList, "typeId_$tId";
        my %typeIds = &runQueryRetHash("SELECT * FROM fileType_$tId");
        $fileTypeIds[$tId] = \%typeIds;
    }
    $fileTypeList = join ',' , @fileTypeList;
    @siteNames = &runQueryRetArray("SELECT name FROM sites");

    if ( ! -l "$baseDir/$thisSite/${thisSite}.ascii" ) {
        `ln -s $baseDir/$thisSite/logs/rt/rtLog.txt $baseDir/$thisSite/${thisSite}.ascii`;
    }
    foreach $siteName ( @siteNames ) {
        $jrnlDir = "$baseDir/$siteName/journal";
        if ( -e "$jrnlDir/loadingActive" ) {
            unlink "$jrnlDir/loadingActive";
        }
        if ( -e "$jrnlDir/inhibitPrepare" ) {
            unlink "$jrnlDir/inhibitPrepare";
        }
    }
}

sub initLoad4OneSite() {
    my ($siteName) = @_;
    my $siteDir = "$baseDir/$siteName";
    if ( ! -d "$siteDir" ) {
        print "directory $siteDir does not exist \n";
        exit;
    }
    
    my $inFN = "$siteDir/$siteName.ascii";
    if ( -l $inFN ) {
        $link = readlink $inFN;
        if ( $link =~ "^/" ) {
             $siteInputFiles{$siteName} = $link;
        } else {
             $siteInputFiles{$siteName} = "$siteDir/$link";
        }
        if ( ! -e $siteInputFiles{$siteName} ) {
             print "Input file $siteInputFiles{$siteName} does not exist \n";
             exit;
        }
    } elsif ( -e  $inFN ) {
        $siteInputFiles{$siteName} = $inFN;
    } else {
        print "Need to supply input file or link to it for site $siteName\n";
        exit;
    }
    ($siteIds{$siteName}, $timeZones{$siteName}, $backupInt, $backupTime) =
                         &runQueryWithRet("SELECT id, timezone, backupInt, backupTime
                                             FROM sites
                                            WHERE name = '$siteName'");
    if ( ! $backupInts{$siteName} ) {
        $backupInts{$siteName} = $backupIntDef;
    }
    if ( ! -d "$siteDir/journal" ) {
        mkdir "$siteDir/journal";
    }
    if ( ! -d "$siteDir/backup" ) {
        mkdir "$siteDir/backup";
    }

    $bkuptime = join "-", split(/ /, $backupTime);
    $backupFile = "$siteDir/backup/${siteName}-${bkuptime}-GMT.backup";
    $backupFiles{$siteName} = $backupFile;

    ($nextFileClose{$siteName}, $nextIdleSessionClose{$siteName}, $nextLongSessionClose{$siteName}) =
                               &runQueryWithRet("SELECT DATE_ADD(closeFileT, INTERVAL $closeFileInt),
                                                        DATE_ADD(closeIdleSessionT, INTERVAL $closeIdleSessionInt),
                                                        DATE_ADD(closeLongSessionT,INTERVAL $closeLongSessionInt)
                                                   FROM sites
                                                  WHERE name = '$siteName' ");
}


sub loadBigFile() {
    my ($siteName, $bigFile) = @_;
    my $inFN = "$baseDir/$siteName/journal/$siteName.ascii";
    my $fileStartTime = &getFileStartTime($siteName, $bigFile);
    my $fileEndTime = &getFileEndTime($siteName, $bigFile);
    if ( ! $fileEndTime) {
        print "No end time found for $bigFile in last 500 lines \n";
        exit;
    }
    # number of minutes covered by bigFile.
    my $nMin = &runQueryWithRet("SELECT TIMESTAMPDIFF(MINUTE,'$fileStartTime', '$fileEndTime')
");
    if ( $nMin < 0 ) {
        $nMin = -$nMin;
    } elsif ( $nMin == 0 ) {
        $nMin = 1;
    }
    my $nRecord = `cat $bigFile|wc -l`;
    my $nRecPerMin = int($nRecord / $nMin) + 1;
    print "nRecord $nRecord nMin $nMin  nRecPerMin $nRecPerMin \n";
    my $loadTime = $fileStartTime;
    my $nr = 0;
    my $nTotalLoaded = 0;
    unless ( open BIGFILE, "< $bigFile" ) {
        print "Can't open file $bigFile for reading \n";
        exit;
    }
    while ( <BIGFILE> ) {
       if ( $nr == 0 ) {
           unless ( open INFILE, "> $inFN" ) {
               print "Can't open file $inFN for writing \n";
               exit;
           }
       }
       # Write one minute's worth of data to INFILE
       print INFILE $_;
       $nr++;
       if ( $nr == $nRecPerMin ) {
           close INFILE;
           # load time is the time coresponding to last record which is assumed to be 1 min
           # after the begin time. If there is no time information in INFILE one minute is
           # added to the previous load time.
           $fileStartTime = &getFileStartTime($siteName, $inFN);
           if ( $fileStartTime ) {
               $loadTime = &runQueryWithRet("SELECT DATE_ADD('$fileStartTime', INTERVAL 1 MINUTE)");
           } else {
               # use the load time from last $inFN.
               $loadTime = &runQueryWithRet("SELECT DATE_ADD('$loadTime', INTERVAL 1 MINUTE)");
           }
           $nrLoaded = &loadOneSite($siteName, $loadTime, 0);
           if ( $nrLoaded < $nr ) {
               print "WARNING: $nrLoaded records loaded out of $nr. \n";
           }
           $loadCounter++;
           $nTotalLoaded += $nrLoaded;
           print "Total loaded: $nTotalLoaded \n";
           $nr = 0;
           &forceClose($siteName, $fileEndTime, 0);
           # _Last... tables are modified only when all big files are loaded.
       }
    }
    close BIGFILE;
    # load the last chunk when it is less than $nRecPerMin.
    if ( $nr > 0 ) {
        close INFILE;
        $nrLoaded = &loadOneSite($siteName, $fileEndTime, 0);
        if ( $nrLoaded < $nr ) {
           print "WARNING: $nrLoaded records loaded out of $nr. \n";
        }
        $loadCounter++;
        $nTotalLoaded += $nr;
        print "Total loaded $nTotalLoaded \n";
        &forceClose($siteName, $fileEndTime, 0);
    }
    $ts = &timestamp();
    print "$ts All done for $bigFile.\n"; 
}

sub loadCloseFile() {
    my ($siteName, $version, $loadLastTables) = @_;

    my $inFile = "$baseDir/$siteName/journal/cfile-V${version}.ascii";
    if ( -z $inFile ) {return;}

    &printNow( "Loading closed files version $version ... ");
    my $mysqlIn = "$baseDir/$siteName/journal/mysqlin.c";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($c, $fileId, $bytesR, $bytesW, $closeT) = split('\t');
        #print "c=$c, id=$fileId, br=$bytesR, bw=$bytesW, t=$closeT\n";

        # find if there is corresponding open file, if not don't bother
        my ($sessionId, $pathId, $openT) = 
	    &runQueryWithRet("SELECT sessionId, pathId, openT 
                              FROM ${siteName}_openedFiles
                              WHERE id = $fileId");
        next if ( ! $sessionId );
        my $jobId = &runQueryWithRet("SELECT jobId
                                       FROM ${siteName}_openedSessions
                                      WHERE id = $sessionId ");
        if ( ! $jobId ) {
             $jobId = &runQueryWithRet("SELECT jobId
                                          FROM ${siteName}_closedSessions
                                         WHERE id = $sessionId ");
        }
        if ( ! $jobId ) {
            print "pathId $pathId with sessionId  $sessionId has no entries in open or closed sessions \n";
            next;
        }
        if ( $version == 1 ) {
            $closeT = &runQueryWithRet("SELECT CONVERT_TZ('$closeT', '$timeZones{$siteName}', 'GMT') ");
        }

        &runQuery("UPDATE ${siteName}_jobs   
                      SET endT = GREATEST( '$closeT', endT)
                    WHERE jobId = $jobId");
        

        # remove it from the open files table
        &runQuery("DELETE FROM ${siteName}_openedFiles WHERE id = $fileId");

        # and insert into the closed
        print MYSQLIN "$fileId \t $sessionId \t $pathId \t  ";
        print MYSQLIN "$openT \t  $closeT \t $bytesR \t $bytesW \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE
               INTO TABLE ${siteName}_closedFiles         ");
    if ( $loadLastTables ) {
        foreach $period ( @periods ) {
            &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE
                       INTO TABLE ${siteName}_closedFiles_Last$period");
        }
    } else {
        &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE
                   INTO TABLE closedFiles"); 
    }
    print "$rows rows loaded \n";
    `rm -f $mysqlIn $inFile`;
}

sub loadCloseSession() {
    my ($siteName, $version, $loadLastTables) = @_;

    my $inFile = "$baseDir/$siteName/journal/dfile-V${version}.ascii";
    if ( -z $inFile ) {return;}

    &printNow( "Loading closed sessions version $version... ");
    my $mysqlIn = "$baseDir/$siteName/journal/mysqlin.d";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;

        my ($d, $sessionId, $sec, $timestamp) = split('\t');
        #print "d=$d, sId=$sessionId, sec=$sec, t=$timestamp\n";


        # find if there is corresponding open session, if not don't bother
        my ($jobId, $userId, $pId, $clientHId, $serverHId) = 
	   &runQueryWithRet("SELECT jobId, userId, pId, clientHId, serverHId 
                             FROM ${siteName}_openedSessions
                             WHERE id = $sessionId");
        next if ( ! $pId  );
        # update jobs table
        if ( $version == 1 ) {
            $timestamp = &runQueryWithRet("SELECT CONVERT_TZ('$timestamp', '$timeZones{$siteName}', 'GMT') ");
        }
        &runQuery("UPDATE ${siteName}_jobs  SET noOpenSessions = noOpenSessions - 1, 
                                                beginT = LEAST(beginT, DATE_SUB('$timestamp', INTERVAL $sec SECOND)),
                                                endT   = GREATEST(endT, '$timestamp')
                                          WHERE jobId  = $jobId");
        
        # remove it from the open session table
        &runQuery("DELETE FROM ${siteName}_openedSessions 
                   WHERE id = $sessionId");

        # and insert into the closed
        print MYSQLIN "$sessionId \t $jobId \t $userId \t $pId \t ";
        print MYSQLIN "$clientHId \t $serverHId \t $sec \t $timestamp \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE
               INTO TABLE ${siteName}_closedSessions          ");
    if ( $loadLastTables ) {
        foreach $period ( @periods ) {
            &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE
                       INTO TABLE ${siteName}_closedSessions_Last$period ");
        }
    } else {
        &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE
                   INTO TABLE closedSessions ");
        }
    print "$rows rows loaded \n";
    `rm -f $mysqlIn $inFile`;
}


sub loadOneSite() {
    my ($siteName, $loadTime, $loadLastTables) = @_;

    my $inFN  = "$baseDir/$siteName/journal/$siteName.ascii";
    # open the input file for reading
    unless ( open INFILE, "< $inFN" ) {
	print "Can't open file $inFN for reading\n";
	return 0;
    }
    $version = &runQueryWithRet("SELECT version
                                   FROM sites
                                  WHERE name = '$siteName' ");
    # read the file, sort the data, close the file
    &openInputFiles($siteName, $version );
    print "Sorting...\n";
    my $nr = 0;
    my @versions = ($version);
    while ( <INFILE> ) {
        if    ( $_ =~ m/^o/ ) { print OFILE $_; }
        elsif ( $_ =~ m/^u/ ) { print UFILE $_; }
        elsif ( $_ =~ m/^d/ ) { print DFILE $_; }
        elsif ( $_ =~ m/^c/ ) { print CFILE $_; }
        elsif ( $_ =~ m/^r/ ) { print RFILE $_; }
        elsif ( $_ =~ m/^v/ ) {
            chomp;
            my ($v, $newVersion) =  split('\t');
            $newVersion = int($newVersion);
            next if ( $newVersion == $version ); 
            $version = $newVersion;
            push @versions, $version;
            &runQuery("UPDATE sites
                          SET version = $version
                        WHERE name = '$siteName' ");
            &closeInputFiles();
            &openInputFiles($siteName, $version );
        }
        $nr++;
    }

    close INFILE;
    `rm -f $inFN`;

    &closeInputFiles();

    print "Loading...\n";
    foreach $version ( @versions ) {
        &makeUniqueFiles($siteName, $version);
        &loadOpenSession($siteName, $loadTime, $version);
        &loadOpenFile($siteName, $version);
        &loadCloseFile($siteName, $version, $loadLastTables);
        &loadCloseSession($siteName, $version, $loadLastTables);
        &loadXrdRestarts($siteName, $version, $loadTime, $loadLastTables );
    }
    # record loadTime in sites table
    &runQuery("UPDATE sites 
                  SET dbUpdate = '$loadTime'
                WHERE name = '$siteName' ");
    print "$nr loaded, load time $loadTime \n";
    return $nr;
}

sub loadOpenFile() {
    my ($siteName, $version) = @_;
    use vars qw($o $fileId $user $pid $clientHost $path $openT $size $srvHost);

    my $inFile = "$baseDir/$siteName/journal/ofile-V${version}.ascii";
    if ( -z $inFile ) {return;}

    &printNow( "Loading opened files version $version... ");
    my $mysqlIn = "$baseDir/$siteName/journal/mysqlin.o";
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;
 
        if ( $version == 1 ) {
            ($o,$fileId,$user,$pid,$clientHost,$path,$openT,$srvHost) = split('\t');
            $size = 0;
        } else {
            ($o,$fileId,$user,$pid,$clientHost,$path,$openT,$size,$srvHost) = split('\t');
        }

        my $sessionId = &findSessionId($user,$pid,$clientHost,$srvHost,$siteName);
        next if ( ! $sessionId ); # error: no corresponding session id

        my $jobId = &runQueryWithRet("SELECT jobId
                                       FROM ${siteName}_openedSessions
                                      WHERE id = $sessionId ");
        if ( $version == 1 ) {
            $openT = &runQueryWithRet("SELECT CONVERT_TZ('$openT', '$timeZones{$siteName}', 'GMT') ");
        }
        
        &runQuery("UPDATE ${siteName}_jobs   
                      SET beginT = LEAST( '$openT', beginT)
                    WHERE jobId = $jobId");
        
        my $pathId = &findOrInsertPathId($path, $size);
        #print "$sessionId $pathId \n";
        if ( ! $pathId ) {
             print "path id not found for $path \n";
            next;
        }
        
        print MYSQLIN "$fileId \t $sessionId \t $pathId \t $openT \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_openedFiles");

    print "$rows rows loaded \n";
    `rm -f $mysqlIn $inFile`;

}

sub loadOpenSession() {
    use vars qw($u  $sessionId  $user  $pid  $clientHost  $srvHost  $connectT);
    my ($siteName, $loadTime, $version) = @_;
    my $inFile = "$baseDir/$siteName/journal/ufile-V${version}.ascii";
    if ( -z $inFile ) {return;}
    &printNow( "Loading open sessions version $version... ");
    my $mysqlIn = "$baseDir/$siteName/journal/mysqlin.u";
    
    open INFILE, "<$inFile" or die "can't open $inFile for read: $!";
    open MYSQLIN, ">$mysqlIn" or die "can't open $mysqlIn for writing: $!";
    my $rows = 0;
    while ( <INFILE> ) {
        chomp;
        if ( $version <= 2 ) {
            ($u, $sessionId, $user, $pid, $clientHost, $srvHost) = split('\t');
            $connectT = $loadTime;
        } else {
            ($u, $sessionId, $user, $pid, $clientHost, $srvHost, $connectT) = split('\t');
        }
        my $userId       = &findOrInsertUserId($user, $siteName);
        my $clientHostId = &findOrInsertHostId($clientHost, $siteName, 2);
        my $serverHostId = &findOrInsertHostId($srvHost, $siteName, 1);
        my $jobId        = &runQueryWithRet("SELECT jobId 
                                               FROM ${siteName}_jobs
                                              WHERE userId    = $userId        AND
                                                    pId       = $pid           AND
                                                    clientHId = $clientHostId  AND
                                                    ( noOpenSessions > 0         OR
                                                      '$connectT' <= DATE_ADD(endT, INTERVAL $maxJobIdleTime) )
                                           ORDER BY jobId DESC
                                              LIMIT 1     ");
        if ( $jobId ) {
            &runQuery("UPDATE ${siteName}_jobs   
                          SET noOpenSessions = noOpenSessions + 1,
                              beginT         = LEAST('$connectT', beginT)
                        WHERE jobId = $jobId");
        } else {
            &runQuery("INSERT IGNORE INTO ${siteName}_jobs ( userId,  pId,  clientHId,   noOpenSessions, beginT,      endT    ) 
                                   VALUES                  ($userId, $pid, $clientHostId,      1       ,'$connectT', '$connectT')");
            $jobId = &runQueryWithRet("SELECT LAST_INSERT_ID()");
        }
        #print "uid=$userId, chid=$clientHostId, shd=$serverHostId, jobId\n";
        print MYSQLIN "$sessionId \t  $jobId \t $userId \t $pid \t ";
        print MYSQLIN "$clientHostId \t $serverHostId \t $connectT \n";
        $rows++;
    }
    close INFILE;
    close MYSQLIN;

    &runQuery("LOAD DATA LOCAL INFILE '$mysqlIn' IGNORE 
               INTO TABLE ${siteName}_openedSessions");
    print "$rows rows loaded \n";
    `rm -f $mysqlIn $inFile`;
}

sub loadXrdRestarts() {
    my ($siteName, $version, $GMTnow, $loadLastTables) = @_;

    my $siteId = $siteIds{$siteName};

    my $inFile = "$baseDir/$siteName/journal/rfile-V${version}.ascii";
    if ( -z $inFile ) {
        `rm -f $inFile`;
        return;
    }
    if ( !open INFILE, "<$inFile" ) {
        return;
    }
    while ( <INFILE> ) {
        chomp;
        my ($r, $hostName, $timestamp) = split('\t');

        my $hostId = $hostIds{$hostName};
        if ( ! $hostId ) {
            $hostId = &runQueryWithRet("SELECT id
                                        FROM ${siteName}_hosts
                                        WHERE hostName = '$hostName'");
	}
        if ( ! $hostId ) {
            print "WARNING: No entry for host name $hostName at $siteName\n";
	    next;
	}
        &runQuery("INSERT IGNORE INTO xrdRestarts(hostId, siteId, startT)
                   VALUES ($hostId, $siteId, '$timestamp')");
        &closeInteruptedSessions($hostId, $siteName, $timestamp, $GMTnow, $loadLastTables);
    }
    `rm -f $inFile`;
}

sub localToGMT() {
    my ($siteName, $localTime) = @_;
    $version = &runQueryWithRet("SELECT version
                                   FROM sites
                                  WHERE name = '$siteName' ");
    if ( $version > 1 ) {
        return($localTime);
    } else {
        return(&runQueryWithRet("SELECT CONVERT_TZ('$localTime', '$timeZones{$siteName}', 'GMT') ") );
    }
}
# opens the <fName>.lock file for writing & locks it (write lock)
sub lockTheFile() {
    my ($fName) = @_;

    my $lockFName = "$fName.lock";
    print "Locking $lockFName...\n";
    unless ( open($lockF, "> $lockFName") ) {
	print "Can't open file $lockFName 4 writing\n";
	return;
    }
    my $lk_parms = pack('sslllll', F_WRLCK, 0, 0, 0, 0, 0, 0);
    fcntl($lockF, F_SETLKW, $lk_parms) or die "can't fcntl F_SETLKW: $!";
    return $lockF;
}
sub makeUniqueFiles() {
    use vars qw($file $type);
    my ($siteName, $version) = @_;
    my $tmpFile = "$baseDir/$siteName/journal/tmp";
    foreach $type ( 'ofile', 'ufile', 'dfile', 'cfile' ) {
       $file = "$baseDir/$siteName/journal/${type}-V${version}.ascii";
       next if ( -z $file);
       `sort -u +1 -2 $file > $tmpFile; mv -f $tmpFile $file`;
    }
}
sub openInputFiles() {
    my ($siteName, $version) = @_;
    my $jrnlDir = "$baseDir/$siteName/journal";
    open OFILE, ">$jrnlDir/ofile-V${version}.ascii" or die "can't open ofile.ascii for write: $!";
    open UFILE, ">$jrnlDir/ufile-V${version}.ascii" or die "can't open ufile.ascii for write: $!";
    open DFILE, ">$jrnlDir/dfile-V${version}.ascii" or die "can't open dfile.ascii for write: $!";
    open CFILE, ">$jrnlDir/cfile-V${version}.ascii" or die "can't open cfile.ascii for write: $!";
    open RFILE, ">$jrnlDir/rfile-V${version}.ascii" or die "can't open rfile.ascii for write: $!";
}

sub printNow() {
    my ($x) = @_;
    my $prev = $|;
    $| = 1;
    print $x;
    $| = $prev;
}

sub printUsage() {
    $opts = join('|', @_);
    die "Usage: $0 <configFile> $opts \n";
}     
sub readConfigFile() {
    my ($confFile, $caller, $print) = @_;
    unless ( open INFILE, "< $confFile" ) {
        print "Can't open file $confFile\n";
        exit;
    }

    $dbName = "";
    $mySQLUser = "";
    $webUser = "";
    $baseDir = "";
    $thisSite = "";
    $ctrPort = 9930;
    $backupIntDef = "1 DAY";
    $fileCloseWaitTime = "10 MINUNTE";
    $maxJobIdleTime = "15 MINUNTE";
    $maxSessionIdleTime = "12 HOUR";
    $maxConnectTime = "70 DAY";
    $closeFileInt = "15 MINUTE";
    $closeIdleSessionInt = "1 HOUR";
    $closeLongSessionInt = "1 DAY";
    $mysqlSocket = '/tmp/mysql.sock';
    $nTopPerfRows = 20;
    $yearlyStats = 0;
    $allYearsStats = 0;
    @flags = ('OFF', 'ON');
    @sites = ();


    while ( <INFILE> ) {
        chomp();
        my ($token, $v1, $v2, $v3, $v4) = split;
        if ( $token eq "dbName:" ) {
            $dbName = $v1;
        } elsif ( $token eq "MySQLUser:" ) {
            $mySQLUser = $v1;
        } elsif ( $token eq "webUser:" ) {
            $webUser = $v1;
        } elsif ( $token eq "MySQLSocket:" ) {
            $mysqlSocket = $v1;
        } elsif ( $token eq "baseDir:" ) {
            $baseDir = $v1;
        } elsif ( $token eq "ctrPort:" ) {
            $ctrPort = $v1;
        } elsif ( $token eq "thisSite:" ) {
            $thisSite = $v1;
        } elsif ( $token eq "site:" ) {
            push @sites, $v1;
            $timezones{$v1} = $v2;
            $firstDates{$v1} = "$v3 $v4";
        } elsif ( $token eq "backupIntDef:" ) {
            $backupIntDef = "$v1 $v2";
        } elsif ( $token eq "backupInt:" ) {
            $backupInts{$v1} = "$v2 $v3";
        } elsif ( $token eq "fileType:" ) {
            push @fileTypes, $v1;
            $maxRowsTypes{$v1} = $v2;
        } elsif ( $token eq "fileCloseWaitTime:" ) {
            $fileCloseWaitTime = "$v1 $v2";
        } elsif ( $token eq "maxJobIdleTime:" ) {
            $maxJobIdleTime = "$v1 $v2";
        } elsif ( $token eq "maxSessionIdleTime:" ) {
            $maxSessionIdleTime = "$v1 $v2";
        } elsif ( $token eq "maxConnectTime:" ) {
            $maxConnectTime = "$v1 $v2";
        } elsif ( $token eq "closeFileInt:" ) {
            $closeFileInt = "$v1 $v2";
        } elsif ( $token eq "closeIdleSessionInt:" ) {
            $closeIdleSessionInt = "$v1 $v2";
        } elsif ( $token eq "closeLongSessionInt:" ) {
            $closeLongSessionInt = "$v1 $v2";
        } elsif ( $token eq "nTopPerfRows:" ) {
            $nTopPerfRows = $v1;
        } elsif ( $token eq "yearlyStats:" ) {
            if ( lc($v1) eq "on" ) {$yearlyStats = 1;}
        } elsif ( $token eq "allYearsStats:" ) {
            if ( lc($v1) eq "on" ) {$allYearsStats = 1;}
        } else {
            print "Invalid entry: $_ \n";
            close INFILE;
            exit;
        }
    }
    close INFILE;
    # check missing tokens
    @missing = ();
    # $baseDir required for all callers except create
    if ( $caller ne "create" and ! $baseDir ) {
         push @missing, "baseDir";
    }

    if ( $caller eq "collector" or $caller eq "create") {
         if ( ! $thisSite) {
            push @missing, "thisSite";    
         }
    } 
    if ( $caller ne  "collector") {
         if ( ! $dbName ) {push @missing, "dbName";}
         if ( ! $mySQLUser ) {push @missing, "MySQLUser";}
    }

    if ( @missing > 0 ) {
       print "Follwing tokens are missing from $confFile \n";
       foreach $token ( @missing ) {
           print "    $token \n";
       }
       exit
    }
    if ( $print ) {
        if ( $caller eq "collector" ) {
             print "  baseDir: $baseDir \n";
             print "  ctrPort: $ctrPort \n";
             print "  thisSite: $thisSite \n";
             return;
        }
        print "  dbName: $dbName  \n";
        print "  MySQLUser: $mySQLUser \n";
        print "  MySQLSocket: $mysqlSocket \n";
        print "  nTopPerfRows: $nTopPerfRows \n";
        if ( $caller eq "create" ) {
             print "  backupIntDef: $backupIntDef \n";
             print "  thisSite: $thisSite \n";
             foreach $site ( @sites ) {
                 print "  site: $site \n";
                 print "     timeZone: $timezones{$site}  \n";
                 print "     firstDate: $firstDates{$site} \n";
                 if ( $backupInts{$site} ) {
                     print "  backupInt: $backupInts{$site} \n";
                 }
             }
             foreach $fileType ( @fileTypes ) {
                 print "  fileType: $fileType $maxRowsTypes{$fileType} \n";
             }
         } else {
             print "  baseDir: $baseDir \n";
             print "  fileCloseWaitTime: $fileCloseWaitTime \n";
             print "  maxJobIdleTime: $maxJobIdleTime \n";
             print "  maxSessionIdleTime: $maxSessionIdleTime \n";
             print "  maxConnectTime: $maxConnectTime \n";
             print "  closeFileInt: $closeFileInt \n";
             print "  closeIdleSessionInt: $closeIdleSessionInt \n";
             print "  closeLongSessionInt: $closeLongSessionInt \n";
             print "  yearlyStats: $flags[$yearlyStats] \n";
             print "  allYearsStats: $flags[$allYearsStats] \n";
         }  
         if ( $caller eq "load" ) {
             foreach $site ( keys %backupInts ) {
                 print "  backupInts: $site $backupInts{$site} \n";
             }
         }
    }        
} 
    
sub recoverLoad4OneSite() {
    my ($siteName) = @_;
    
    # recover data for the site
    my $jrnlDir = "$baseDir/$siteName/journal";
    my $nr = 0;
    my $inFN = "$jrnlDir/$siteName.ascii";
    my ($version, $loadTime) = &runQueryWithRet("SELECT version, DATE_ADD(dbUpdate, INTERVAL 1 MINUTE) 
                                                   FROM sites
                                                  WHERE name = '$siteName' ");
    # recover u/o/d/c/r files BUT NOT if input file exists in jrnl directory
    # since they will be remade when doLoading is first called
    my $loadedSomething = 0;
    if ( ! -e $inFN ) {
         foreach $v ( 1 .. $version ) {
             print "Checking for pending u/o/d/c/r files for $siteName version $v \n";
             if ( -e  "$jrnlDir/ufile-V${v}.ascii" ) {
                 &loadOpenSession($siteName, $loadTime, $v);
                 $loadedSomething = 1;
             }
             if ( -e  "$jrnlDir/ofile-V${v}.ascii" ) {
                 &loadOpenFile($siteName, $v);
                 $loadedSomething = 1;
             }
             if ( -e  "$jrnlDir/cfile-V${v}.ascii" ) {
                 &loadCloseFile($siteName, $v, 1);
                 $loadedSomething = 1;
             }
             if ( -e  "$jrnlDir/dfile-V${v}.ascii" ) {
                 &loadCloseSession($siteName, $v, 1);
             }
                 $loadedSomething = 1;
             if ( -e  "$jrnlDir/rfile-V${v}.ascii" ) {
                 &loadXrdRestarts($siteName, $v, $loadTime, 1);
                 $loadedSomething = 1;
             }
         }
    } else {
         $nr += &loadOneSite($siteName, $loadTime, 1);
         $loadedSomething = 1;
    }    
    if ( $loadedSomething ) {
         &runQuery("UPDATE sites 
                       SET dbUpdate = '$loadTime'
                     WHERE name = '$siteName' ");
    }
    # backup the backlog file
    my $gmts = &gmtimestamp();
    &backupOneSite($siteName, $gmts);

    # load the backlog.
    $backlogFile = "$jrnlDir/backlog.ascii";

    if ( ! -e $inFN or -z $inFN ) {
        return if (! -e $backlogFile);
    } else {
        `touch $backlogFile; cat $inFN >> $backlogFile; rm -f $inFN`;
    }

    # create temporary tables to store new closed files and sessions.
    &runQuery("CREATE TEMPORARY TABLE closedSessions LIKE ${siteName}_closedSessions");
    &runQuery("CREATE TEMPORARY TABLE closedFiles LIKE ${siteName}_closedFiles");

    &loadBigFile($siteName, $backlogFile); 

    &updateLastTables($siteName);
    &runQuery("DROP TABLE IF EXISTS closedSessions");
    &runQuery("DROP TABLE IF EXISTS closedFiles");

    `rm -f $backlogFile`;
    print "removed  $backlogFile \n";
    if ( -e $stopFName ) {
        stopLoading;
    }
}           
sub returnHash() {
    ($_) = @_;
    my @primes = (101, 127, 157, 181, 199, 223, 239, 251, 271, 307);
    my $i = 1;
    tr/0-9a-zA-Z/0-90-90-90-90-90-90-1/;
    tr/0-9//cd;
    my $hashValue = 0;
    foreach $char ( split / */ ) {
	$i++;
	$hashValue += $i * $primes[$char];
    }
    return $hashValue;
}
sub runQuery() {
    my ($sql) = @_;
#    print "$sql;\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
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

sub runQueryRetHash() {
    my $sql = shift @_;
    my %theHash = ();

#    print "$sql;\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";

    while ( @x = $sth->fetchrow_array ) {
	$theHash{$x[0]} = $x[1];
    }
    return %theHash;
}

sub runQueryRetNum() {
    my $sql = shift @_;
#    print "$sql;\n";
    my $num = $dbh-> do ($sql) or die "Failed to exec \"$sql\", $DBI::errstr";
    return $num;
}

sub runQueryWithRet() {
    my $sql = shift @_;
#    print "$sql;\n";
    my $sth = $dbh->prepare($sql) 
        or die "Can't prepare statement $DBI::errstr\n";
    $sth->execute or die "Failed to exec \"$sql\", $DBI::errstr";
    return $sth->fetchrow_array;
}
sub stopLoading() {
     print "Detected $stopFName. Exiting... \n";
     unlink $stopFName;
     
     foreach $siteName (@siteNames) {
         if ( -e "$baseDir/$siteName/journal/loadingActive" ) {
             unlink "$baseDir/$siteName/journal/loadingActive";
         }
         if ( -e "$baseDir/$siteName/journal/inhibitPrepare" ) {
             unlink "$baseDir/$siteName/journal/inhibitPrepare";
         }
     }
     $dbh->disconnect();
     exit;
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

sub unlockTheFile() {
    my ($fh) = @_;
    my $lk_parms = pack('sslllll', F_UNLCK, 0, 0, 0, 0, 0, 0);
    fcntl($fh, F_SETLKW, $lk_parms);
}
sub updateForClosedSessions() {
    my ($cs, $siteName, $loadTime, $loadLastTables) = @_;

    #insert contents of $cs table into closedSessions tables, delete them 
    # from openSession table and update the jobs table

    &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions
                           SELECT *
                             FROM $cs ");

    &runQuery("DELETE FROM ${siteName}_openedSessions os
                     USING ${siteName}_openedSessions os, $cs cs
                     WHERE os.id = cs.id ");

    &runQuery("CREATE TEMPORARY TABLE IF NOT EXISTS ns ( jobId INT UNSIGNED NOT NULL PRIMARY KEY,
                                                           nos SMALLINT NOT NULL,
                                                           INDEX(nos)                            ) 
                                                           MAX_ROWS=65535                         ");

    &runQuery("INSERT INTO ns
                    SELECT jobId, count(jobId)
                      FROM $cs
                  GROUP BY jobId ");

    &runQuery("UPDATE ${siteName}_jobs j, ns
                  SET noOpenSessions = noOpenSessions - nos
                WHERE j.jobId = ns.jobId  ");

    if ( $loadLastTables ) {
        foreach $period ( @periods ) {
            &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions_Last$period
                            SELECT *
                              FROM $cs
                             WHERE disconnectT > DATE_SUB('$loadTime', INTERVAL 1 $period) ");
        }
    } else {
        &runQuery("INSERT IGNORE INTO closedSessions
                               SELECT *
                                 FROM $cs ");
    }

    &runQuery("DROP TABLE IF EXISTS ns");
}
sub updateLastTables() {
    my ($siteName) = @_;
    my $gmts = &gmtimestamp();
    foreach $period ( @periods ) {
        &runQuery("DELETE FROM ${siteName}_closedSessions_Last$period
                    WHERE disconnectT < DATE_SUB('$gmts', INTERVAL 1 $period) ");

        &runQuery("INSERT IGNORE INTO ${siteName}_closedSessions_Last$period
                   SELECT * 
                     FROM closedSessions
                    WHERE disconnectT >= DATE_SUB('$gmts', INTERVAL 1 $period) ");

        &runQuery("DELETE FROM ${siteName}_closedFiles_Last$period
                    WHERE closeT < DATE_SUB('$gmts', INTERVAL 1 $period) ");

        &runQuery("INSERT IGNORE INTO ${siteName}_closedFiles_Last$period
                   SELECT * 
                     FROM closedFiles
                    WHERE closeT >= DATE_SUB('$gmts', INTERVAL 1 $period) ");
     }
}





