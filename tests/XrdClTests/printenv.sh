#!/bin/bash

function printEnv()
{
  if [ $# -ne 1 ]; then
    echo "[!] Invalid invocation; need a parameter"
    return 1;
  fi

  eval VALUE=\$$1
  printf "%-30s: " $1
  if test x"$VALUE" == x; then
    echo "default"
  else
    echo $VALUE;
  fi
}

printEnv XRDTEST_MAINSERVERURL
printEnv XRDTEST_DISKSERVERURL
printEnv XRDTEST_DATAPATH
printEnv XRDTEST_LOCALFILE
printEnv XRDTEST_REMOTEFILE
printEnv XRDTEST_MULTIIPSERVERURL
