#!/usr/bin/perl

###############################################################################
#                                                                             #
#                              generateSchema.pl                              #
#                                                                             #
#  (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  #
#                             All Rights Reserved                             #
#        Produced by Jacek Becla for Stanford University under contract       #
#               DE-AC02-76SF00515 with the Department of Energy               #
###############################################################################

# $Id$


if ( @ARGV ne 1 ) {
    print "Expected arg: <db name>\n";
    exit;
}
 
my $dbName = $ARGV[0];

@sites     = ("SLAC"  , "RAL"   );
@timezones = ("-08:00", "+00:00");


print "

# DROP DATABASE IF EXISTS $dbName;
# CREATE DATABASE IF NOT EXISTS $dbName;

USE $dbName;


CREATE TABLE IF NOT EXISTS sites (
  id            TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  name          VARCHAR(32),
  timezone      CHAR(6)
);

# one row per minute, keeps last 60 minutes
CREATE TABLE IF NOT EXISTS statsLastHour (
  seqNo         TINYINT UNSIGNED NOT NULL,
  siteId        TINYINT UNSIGNED NOT NULL,
  date          DATETIME,
  noJobs        SMALLINT,
  noUsers       SMALLINT,
  noUniqueF     INT,
  noNonUniqueF  INT,
  minJobs       SMALLINT,
  minUsers      SMALLINT,
  minUniqueF    INT,
  minNonUniqueF INT,
  maxJobs       SMALLINT,
  maxUsers      SMALLINT,
  maxUniqueF    INT,
  maxNonUniqueF INT,
  PRIMARY KEY (seqNo, siteId),
  INDEX (date)
);

# one row per hour, keeps last 24 hours
CREATE TABLE IF NOT EXISTS statsLastDay (
  seqNo         TINYINT UNSIGNED NOT NULL,
  siteId        TINYINT UNSIGNED NOT NULL,
  date          DATETIME,
  noJobs        SMALLINT,
  noUsers       SMALLINT,
  noUniqueF     INT,
  noNonUniqueF  INT,
  minJobs       SMALLINT,
  minUsers      SMALLINT,
  minUniqueF    INT,
  minNonUniqueF INT,
  maxJobs       SMALLINT,
  maxUsers      SMALLINT,
  maxUniqueF    INT,
  maxNonUniqueF INT,
  PRIMARY KEY (seqNo, siteId),
  INDEX (date)
);

# one row per day, keeps last 7 days
CREATE TABLE IF NOT EXISTS statsLastWeek (
  seqNo         TINYINT UNSIGNED NOT NULL,
  siteId        TINYINT UNSIGNED NOT NULL,
  date          DATETIME,
  noJobs        SMALLINT,
  noUsers       SMALLINT,
  noUniqueF     INT,
  noNonUniqueF  INT,
  minJobs       SMALLINT,
  minUsers      SMALLINT,
  minUniqueF    INT,
  minNonUniqueF INT,
  maxJobs       SMALLINT,
  maxUsers      SMALLINT,
  maxUniqueF    INT,
  maxNonUniqueF INT,
  PRIMARY KEY (seqNo, siteId),
  INDEX (date)
);

# one row per day, keeps last 31 days
CREATE TABLE IF NOT EXISTS statsLastMonth (
  seqNo         TINYINT UNSIGNED NOT NULL,
  siteId        TINYINT UNSIGNED NOT NULL,
  date          DATETIME,
  noJobs        SMALLINT,
  noUsers       SMALLINT,
  noUniqueF     INT,
  noNonUniqueF  INT,
  minJobs       SMALLINT,
  minUsers      SMALLINT,
  minUniqueF    INT,
  minNonUniqueF INT,
  maxJobs       SMALLINT,
  maxUsers      SMALLINT,
  maxUniqueF    INT,
  maxNonUniqueF INT,
  PRIMARY KEY (seqNo, siteId),
  INDEX (date)
);

# one row per month, growing indefinitely
CREATE TABLE IF NOT EXISTS statsAllMonths (
  siteId        TINYINT UNSIGNED NOT NULL,
  date          DATETIME,
  noJobs        SMALLINT,
  noUsers       SMALLINT,
  noUniqueF     INT,
  noNonUniqueF  INT,
  minJobs       SMALLINT,
  minUsers      SMALLINT,
  minUniqueF    INT,
  minNonUniqueF INT,
  maxJobs       SMALLINT,
  maxUsers      SMALLINT,
  maxUniqueF    INT,
  maxNonUniqueF INT,
  INDEX (date)
);

# reflects changes since last entry, and last update
CREATE TABLE IF NOT EXISTS rtChanges (
  siteId        TINYINT UNSIGNED NOT NULL,
  jobs          SMALLINT,
  jobs_p        FLOAT,
  users         SMALLINT,
  users_p       FLOAT,
  uniqueF       SMALLINT,
  uniqueF_p     FLOAT,
  nonUniqueF    SMALLINT,
  nonUniqueF_p  FLOAT,
  lastUpdate    DATETIME
);

CREATE TABLE IF NOT EXISTS paths (
  id            MEDIUMINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  typeId        TINYINT NOT NULL,   # BaBar specific
  skimId        TINYINT NOT NULL,   # BaBar specific
  size          BIGINT  NOT NULL DEFAULT 0,
  hash          MEDIUMINT NOT NULL DEFAULT 0,
  name          VARCHAR(255) NOT NULL,
  INDEX (typeId),
  INDEX (skimId),
  INDEX (hash)
);

# BaBar specific!
# e.g.: SP, PR, SPskims, PRskims
CREATE TABLE IF NOT EXISTS fileTypes (
  name         VARCHAR(16),
  id           TINYINT NOT NULL AUTO_INCREMENT PRIMARY KEY
);

# BaBar specific!
CREATE TABLE IF NOT EXISTS skimNames (
  name         VARCHAR(32),
  id           SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY
);

";


foreach $site (@sites) {
    print "

################ ${site} ################

CREATE TABLE IF NOT EXISTS ${site}_openedSessions (
  id            MEDIUMINT UNSIGNED NOT NULL PRIMARY KEY,
  userId        SMALLINT  UNSIGNED NOT NULL,
  pId           SMALLINT  UNSIGNED NOT NULL,
  clientHId     SMALLINT  UNSIGNED NOT NULL,
  serverHId     SMALLINT  UNSIGNED NOT NULL,
  INDEX (userId),
  INDEX (pId),
  INDEX (clientHId),
  INDEX (serverHId)
);

CREATE TABLE IF NOT EXISTS ${site}_closedSessions_LastHour (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  userId        SMALLINT  UNSIGNED NOT NULL,
  pId           SMALLINT  UNSIGNED NOT NULL,
  clientHId     SMALLINT  UNSIGNED NOT NULL,
  serverHId     SMALLINT  UNSIGNED NOT NULL,
  duration      MEDIUMINT UNSIGNED NOT NULL,
  disconnectT   DATETIME  NOT NULL,
  INDEX (userId),
  INDEX (pId),
  INDEX (clientHId),
  INDEX (serverHId),
  INDEX (disconnectT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedSessions_LastDay (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  userId        SMALLINT  UNSIGNED NOT NULL,
  pId           SMALLINT  UNSIGNED NOT NULL,
  clientHId     SMALLINT  UNSIGNED NOT NULL,
  serverHId     SMALLINT  UNSIGNED NOT NULL,
  duration      MEDIUMINT UNSIGNED NOT NULL,
  disconnectT   DATETIME  NOT NULL,
  INDEX (userId),
  INDEX (pId),
  INDEX (clientHId),
  INDEX (serverHId),
  INDEX (disconnectT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedSessions_LastWeek (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  userId        SMALLINT  UNSIGNED NOT NULL,
  pId           SMALLINT  UNSIGNED NOT NULL,
  clientHId     SMALLINT  UNSIGNED NOT NULL,
  serverHId     SMALLINT  UNSIGNED NOT NULL,
  duration      MEDIUMINT UNSIGNED NOT NULL,
  disconnectT   DATETIME  NOT NULL,
  INDEX (userId),
  INDEX (pId),
  INDEX (clientHId),
  INDEX (serverHId),
  INDEX (disconnectT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedSessions_LastMonth (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  userId        SMALLINT  UNSIGNED NOT NULL,
  pId           SMALLINT  UNSIGNED NOT NULL,
  clientHId     SMALLINT  UNSIGNED NOT NULL,
  serverHId     SMALLINT  UNSIGNED NOT NULL,
  duration      MEDIUMINT UNSIGNED NOT NULL,
  disconnectT   DATETIME  NOT NULL,
  INDEX (userId),
  INDEX (pId),
  INDEX (clientHId),
  INDEX (serverHId),
  INDEX (disconnectT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedSessions_LastYear (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  userId        SMALLINT  UNSIGNED NOT NULL,
  pId           SMALLINT  UNSIGNED NOT NULL,
  clientHId     SMALLINT  UNSIGNED NOT NULL,
  serverHId     SMALLINT  UNSIGNED NOT NULL,
  duration      MEDIUMINT UNSIGNED NOT NULL,
  disconnectT   DATETIME  NOT NULL,
  INDEX (userId),
  INDEX (pId),
  INDEX (clientHId),
  INDEX (serverHId),
  INDEX (disconnectT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedSessions_2005 (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  userId        SMALLINT  UNSIGNED NOT NULL,
  pId           SMALLINT  UNSIGNED NOT NULL,
  clientHId     SMALLINT  UNSIGNED NOT NULL,
  serverHId     SMALLINT  UNSIGNED NOT NULL,
  duration      MEDIUMINT UNSIGNED NOT NULL,
  disconnectT   DATETIME  NOT NULL,
  INDEX (userId),
  INDEX (pId),
  INDEX (clientHId),
  INDEX (serverHId),
  INDEX (disconnectT)
);

CREATE TABLE IF NOT EXISTS ${site}_openedFiles (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  sessionId     INT       UNSIGNED NOT NULL,
  pathId        MEDIUMINT UNSIGNED NOT NULL,
  openT         DATETIME  NOT NULL,
  INDEX (sessionId),
  INDEX (pathId),
  INDEX (openT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedFiles_LastHour (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  sessionId     INT       UNSIGNED NOT NULL,
  pathId	MEDIUMINT UNSIGNED NOT NULL,
  openT         DATETIME  NOT NULL,
  closeT        DATETIME  NOT NULL,
  bytesR        BIGINT    UNSIGNED NOT NULL,
  bytesW        BIGINT    UNSIGNED NOT NULL,
  INDEX (sessionId),
  INDEX (pathId),
  INDEX (closeT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedFiles_LastDay (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  sessionId     INT       UNSIGNED NOT NULL,
  pathId	MEDIUMINT UNSIGNED NOT NULL,
  openT         DATETIME  NOT NULL,
  closeT        DATETIME  NOT NULL,
  bytesR        BIGINT    UNSIGNED NOT NULL,
  bytesW        BIGINT    UNSIGNED NOT NULL,
  INDEX (sessionId),
  INDEX (pathId),
  INDEX (closeT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedFiles_LastWeek (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  sessionId     INT       UNSIGNED NOT NULL,
  pathId	MEDIUMINT UNSIGNED NOT NULL,
  openT         DATETIME  NOT NULL,
  closeT        DATETIME  NOT NULL,
  bytesR        BIGINT    UNSIGNED NOT NULL,
  bytesW        BIGINT    UNSIGNED NOT NULL,
  INDEX (sessionId),
  INDEX (pathId),
  INDEX (closeT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedFiles_LastMonth (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  sessionId     INT       UNSIGNED NOT NULL,
  pathId	MEDIUMINT UNSIGNED NOT NULL,
  openT         DATETIME  NOT NULL,
  closeT        DATETIME  NOT NULL,
  bytesR        BIGINT    UNSIGNED NOT NULL,
  bytesW        BIGINT    UNSIGNED NOT NULL,
  INDEX (sessionId),
  INDEX (pathId),
  INDEX (closeT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedFiles_LastYear (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  sessionId     INT       UNSIGNED NOT NULL,
  pathId	MEDIUMINT UNSIGNED NOT NULL,
  openT         DATETIME  NOT NULL,
  closeT        DATETIME  NOT NULL,
  bytesR        BIGINT    UNSIGNED NOT NULL,
  bytesW        BIGINT    UNSIGNED NOT NULL,
  INDEX (sessionId),
  INDEX (pathId),
  INDEX (closeT)
);

CREATE TABLE IF NOT EXISTS ${site}_closedFiles_2005 (
  id            INT       UNSIGNED NOT NULL PRIMARY KEY,
  sessionId     INT       UNSIGNED NOT NULL,
  pathId	MEDIUMINT UNSIGNED NOT NULL,
  openT         DATETIME  NOT NULL,
  closeT        DATETIME  NOT NULL,
  bytesR        BIGINT    UNSIGNED NOT NULL,
  bytesW        BIGINT    UNSIGNED NOT NULL,
  INDEX (sessionId),
  INDEX (pathId),
  INDEX (closeT)
);


# compressed info for top performers (top users)
CREATE TABLE IF NOT EXISTS ${site}_topPerfUsersNow (
  theId      INT NOT NULL,    # user Id
  jobs       INT NOT NULL,
  files      INT NOT NULL,
  fSize      INT NOT NULL     # [MB]
);
CREATE TABLE IF NOT EXISTS ${site}_topPerfUsersPast (
  theId      INT NOT NULL,    # user Id
  jobs       INT NOT NULL,
  files      INT NOT NULL,
  fSize      INT NOT NULL,    # [MB]
  volume     INT NOT NULL,
  timePeriod CHAR(6)          # \"hour\", \"week\", \"month\", \"year\"
);

# compressed info for top performers (top skims)
CREATE TABLE IF NOT EXISTS ${site}_topPerfSkimsNow (
  theId      INT NOT NULL,    # skim Id
  jobs       INT NOT NULL,
  files      INT NOT NULL,
  fSize      INT NOT NULL,    # [MB]
  users      INT NOT NULL
);
CREATE TABLE IF NOT EXISTS ${site}_topPerfSkimsPast (
  theId      INT NOT NULL,    # skim Id
  jobs       INT NOT NULL,
  files      INT NOT NULL,
  fSize      INT NOT NULL,    # [MB]
  users      INT NOT NULL,
  volume     INT NOT NULL,
  timePeriod CHAR(6)          # \"hour\", \"week\", \"month\", \"year\"
);

# compressed info for top performers (top files)
CREATE TABLE IF NOT EXISTS ${site}_topPerfFilesNow (
  theId      INT NOT NULL,    # path Id
  jobs       INT NOT NULL,
  fSize      INT NOT NULL     # [MB]
);
CREATE TABLE IF NOT EXISTS ${site}_topPerfFilesPast (
  theId      INT NOT NULL,    # path Id
  jobs       INT NOT NULL,
  fSize      INT NOT NULL,    # [MB]
  volume     INT NOT NULL,
  timePeriod CHAR(6)          # \"hour\", \"week\", \"month\", \"year\"
);

# compressed info for top performers (top skims)
CREATE TABLE IF NOT EXISTS ${site}_topPerfTypesNow (
  theId      INT NOT NULL,    # type Id
  jobs       INT NOT NULL,
  files      INT NOT NULL,
  fSize      INT NOT NULL,    # [MB]
  users      INT NOT NULL
);
CREATE TABLE IF NOT EXISTS ${site}_topPerfTypesPast (
  theId      INT NOT NULL,    # type Id
  jobs       INT NOT NULL,
  files      INT NOT NULL,
  fSize      INT NOT NULL,    # [MB]
  users      INT NOT NULL,
  volume     INT NOT NULL,
  timePeriod CHAR(6)          # \"hour\", \"week\", \"month\", \"year\"
);

CREATE TABLE IF NOT EXISTS ${site}_users (
  id            SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  name          VARCHAR(24) NOT NULL
);

CREATE TABLE IF NOT EXISTS ${site}_hosts (
  id            SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  hostName      VARCHAR(64) NOT NULL
);

";
} ### end of site-specific tables ###

