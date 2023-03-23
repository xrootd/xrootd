#!/bin/bash

# set the TEST_SIGNING
export TEST_SIGNING=1

if [[ ${HOSTNAME} = 'metaman' ]] ; then
  # download the a test file for upload tests
  mkdir -p /data
  cp /downloads/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat /data/testFile.dat
  chown -R xrootd:xrootd /data
fi

# the data nodes
datanodes=('srv1' 'srv2' 'srv3' 'srv4')

# create 'bigdir' in each of the data nodes
if [[ ${datanodes[*]} =~ ${HOSTNAME} ]] ; then
  mkdir -p /data/bigdir
  cd /data/bigdir
  for i in `seq 10000`; do touch `uuidgen`.dat; done
  cd - >/dev/null
fi

# download the test files for 'srv1'
if [[ ${HOSTNAME} = 'srv1' ]] ; then
  cp /downloads/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat /data
  cp /downloads/b3d40b3f-1d15-4ad3-8cb5-a7516acb2bab.dat /data
  cp /downloads/b74d025e-06d6-43e8-91e1-a862feb03c84.dat /data
  cp /downloads/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat /data
  cp /downloads/cef4d954-936f-4945-ae49-60ec715b986e.dat /data
  mkdir /data/metalink
  cp /downloads/input*.meta* /data/metalink/
  cp /downloads/ml*.meta*    /data/metalink/
fi

# download the test files for 'srv2' and add another instance on 1099
if [[ ${HOSTNAME} = 'srv2' ]] ; then
  cp /downloads/1db882c8-8cd6-4df1-941f-ce669bad3458.dat /data
  cp /downloads/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat /data
  cp /downloads/7235b5d1-cede-4700-a8f9-596506b4cc38.dat /data
  cp /downloads/7e480547-fe1a-4eaf-a210-0f3927751a43.dat /data
  cp /downloads/89120cec-5244-444c-9313-703e4bee72de.dat /data
fi

# download the test files for 'srv3'
if [[ ${HOSTNAME} = 'srv3' ]] ; then
  cp /downloads/1db882c8-8cd6-4df1-941f-ce669bad3458.dat /data
  cp /downloads/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat /data
  cp /downloads/89120cec-5244-444c-9313-703e4bee72de.dat /data
  cp /downloads/b74d025e-06d6-43e8-91e1-a862feb03c84.dat /data
  cp /downloads/cef4d954-936f-4945-ae49-60ec715b986e.dat /data
fi

# download the test files for 'srv4'
if [[ ${HOSTNAME} = 'srv4' ]] ; then
  cp /downloads/1db882c8-8cd6-4df1-941f-ce669bad3458.dat /data
  cp /downloads/7e480547-fe1a-4eaf-a210-0f3927751a43.dat /data
  cp /downloads/89120cec-5244-444c-9313-703e4bee72de.dat /data
  cp /downloads/b74d025e-06d6-43e8-91e1-a862feb03c84.dat /data
  cp /downloads/cef4d954-936f-4945-ae49-60ec715b986e.dat /data
  cp /downloads/data.zip                                 /data
  cp /downloads/large.zip                                /data
fi

# make sure the test files and directories are owned by 'xrootd' user
if [[ ${datanodes[*]} =~ ${HOSTNAME} ]] ; then
  chown -R xrootd:xrootd /data
fi
