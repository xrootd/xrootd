#!/bin/bash
source /etc/XrdTest/utils/functions.sh
log "Initializing test case on slave" @slavename@ "..."

if [[ @slavename@ =~ client1 ]]; then
  PYVER=2.4.3
elif [[ @slavename@ =~ client2 ]]; then
  PYVER=2.5.6
elif [[ @slavename@ =~ client3 ]]; then
  PYVER=2.6.6
elif [[ @slavename@ =~ client4 ]]; then
  PYVER=2.7.3
elif [[ @slavename@ =~ client5 ]]; then
  PYVER=3.0.1
elif [[ @slavename@ =~ client6 ]]; then
  PYVER=3.1.4
elif [[ @slavename@ =~ client7 ]]; then
  PYVER=3.2.3
elif [[ @slavename@ =~ client8 ]]; then
  PYVER=3.3.0
fi

if [[ @slavename@ =~ client ]]; then
  PYVER_SHORT=${PYVER%.*}
  PYTHON=/usr/local/bin/python$PYVER_SHORT

  #-----------------------------------------------------------------------------
  log "Installing dependencies ..."
  yum -y -q install gcc gcc-c++ make zlib zlib-devel

  #-----------------------------------------------------------------------------
  log "Installing custom Python version ..."
  cd /tmp

  if [ ! -d "Python-$PYVER" ]; then
    curl -sSkO "http://www.python.org/ftp/python/$PYVER/Python-$PYVER.tar.bz2" > /dev/null
    tar -xf Python-$PYVER.tar.bz2
  fi

  cd Python-$PYVER
  ./configure -q && make -s && make install BATCH=yes

  #-----------------------------------------------------------------------------
  log "Fetching latest pyxrootd build ..."

  cd /tmp
  rm -rf pyxrootd.zip xrootd-python-master
  wget -q https://github.com/xrootd/xrootd-python/archive/master.zip -O pyxrootd.zip
  unzip -qq pyxrootd.zip && cd xrootd-python-master
  $PYTHON setup.py install

  #-----------------------------------------------------------------------------
  log "Installing pytest ..."

  curl http://python-distribute.org/distribute_setup.py | $PYTHON
  /usr/local/bin/easy_install-$PYVER_SHORT pytest

fi

