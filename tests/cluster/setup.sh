#!/usr/bin/env bash

######
# Starts a cluster configuration locally, instead of using
# containers.
#####

set -e

: ${XROOTD:=$(command -v xrootd)}
: ${CMSD:=$(command -v cmsd)}
: ${OPENSSL:=$(command -v openssl)}
: ${CRC32C:=$(command -v xrdcrc32c)}
: ${STAT:=$(command -v stat)}

servernames=("metaman" "man1" "man2" "srv1" "srv2" "srv3" "srv4")
datanodes=("srv1" "srv2" "srv3" "srv4")

DATAFOLDER="./data"
TMPDATAFOLDER="./rout"
PREDEF="./mvdata"

filenames=("1db882c8-8cd6-4df1-941f-ce669bad3458.dat"
       "3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat"
       "7235b5d1-cede-4700-a8f9-596506b4cc38.dat"
       "7e480547-fe1a-4eaf-a210-0f3927751a43.dat"
       "89120cec-5244-444c-9313-703e4bee72de.dat"
       "a048e67f-4397-4bb8-85eb-8d7e40d90763.dat"
       "b3d40b3f-1d15-4ad3-8cb5-a7516acb2bab.dat"
       "b74d025e-06d6-43e8-91e1-a862feb03c84.dat"
       "cb4aacf1-6f28-42f2-b68a-90a73460f424.dat"
       "cef4d954-936f-4945-ae49-60ec715b986e.dat")

filesize() {
	case $(uname) in
	Darwin) ${STAT} -f"%z" $1 ;;
	Linux)  ${STAT} -c"%s" $1 ;;
	*)      ${STAT} -c"%s" $1 ;;
	esac
}

formatfiles() {
	case $(uname) in
              Darwin)
               sed -i '' "s|<size>.*</size>|<size>$new_size</size>|" $1
               sed -i '' "s|<hash type=\"crc32c\">.*</hash>|<hash type=\"crc32c\">$new_hash</hash>|" $1
               sed -i '' "s|<hash type=\"zcrc32\">.*</hash>|<hash type=\"crc32c\">$new_hash</hash>|" $1
               ;;
              Linux)
               sed -i "s|<size>.*</size>|<size>$new_size</size>|" $1
               sed -i "s|<hash type=\"crc32c\">.*</hash>|<hash type=\"crc32c\">$new_hash</hash>|" $1
               sed -i "s|<hash type=\"zcrc32\">.*</hash>|<hash type=\"crc32c\">$new_hash</hash>|" $1
               ;;
              *)
               sed -i "s|<size>.*</size>|<size>$new_size</size>|" $1
               sed -i "s|<hash type=\"crc32c\">.*</hash>|<hash type=\"crc32c\">$new_hash</hash>|" $1
               sed -i "s|<hash type=\"zcrc32\">.*</hash>|<hash type=\"crc32c\">$new_hash</hash>|" $1
               ;;
	esac
}

generate(){

       # The fixtures below cost a 2GB openssl rand, 4x1000 uuidgen calls and
       # ten 16MB files, so they are generated once and cached for the life of
       # the build tree. The sentinel is written last, which is the point: an
       # interrupted generation leaves no sentinel and is redone, where testing
       # for the data directory itself would treat a half-built tree as done.
       if [[ -e ${DATAFOLDER}/.generated ]]; then
              return
       fi

       rm -rf ${DATAFOLDER}
       mkdir -p ${TMPDATAFOLDER}

       for i in ${filenames[@]}; do
              ${OPENSSL} rand -out "${TMPDATAFOLDER}/${i}" $(( 2**24 ))
       done

       # correct the info inside of metalink files
       insertFileInfo

       # create local srv directories
       echo "Creating directories for each instance..."

       for i in ${servernames[@]}; do
              mkdir -p ${DATAFOLDER}/${i}/data
       done

       # create large file for reading in one request with max size readv
       ${OPENSSL} rand -out "${DATAFOLDER}/srv1/data/2GB.dat" $((2**31 - 1))

       for i in ${datanodes[@]}; do
              mkdir -p ${DATAFOLDER}/${i}/data/bigdir
              cd ${DATAFOLDER}/${i}/data/bigdir
              for i in `seq 1000`;
                     do touch `uuidgen`.dat;
              done
              cd - >/dev/null
       done

       for i in ${servernames[@]}; do
              if [[ ${i} == 'metaman' ]] ; then
                     # download the a test file for upload tests
                     mkdir -p ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat ${DATAFOLDER}/${i}/data/testFile.dat
              fi

              # download the test files for 'srv1'
              if [[ ${i} == 'srv1' ]] ; then
                     cp ${TMPDATAFOLDER}/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/b3d40b3f-1d15-4ad3-8cb5-a7516acb2bab.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/b74d025e-06d6-43e8-91e1-a862feb03c84.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/cef4d954-936f-4945-ae49-60ec715b986e.dat ${DATAFOLDER}/${i}/data
                     mkdir -p ${DATAFOLDER}/${i}/data/metalink
                     cp ${PREDEF}/input*.meta* ${DATAFOLDER}/${i}/data/metalink/
                     cp ${PREDEF}/ml*.meta*    ${DATAFOLDER}/${i}/data/metalink/
              fi

              # download the test files for 'srv2' and add another instance on 1099
              if [[ ${i} == 'srv2' ]] ; then
                     cp ${TMPDATAFOLDER}/1db882c8-8cd6-4df1-941f-ce669bad3458.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/7235b5d1-cede-4700-a8f9-596506b4cc38.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/7e480547-fe1a-4eaf-a210-0f3927751a43.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/89120cec-5244-444c-9313-703e4bee72de.dat ${DATAFOLDER}/${i}/data
              fi

              # download the test files for 'srv3'
              if [[ ${i} == 'srv3' ]] ; then
                     cp ${TMPDATAFOLDER}/1db882c8-8cd6-4df1-941f-ce669bad3458.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/89120cec-5244-444c-9313-703e4bee72de.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/b74d025e-06d6-43e8-91e1-a862feb03c84.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/cef4d954-936f-4945-ae49-60ec715b986e.dat ${DATAFOLDER}/${i}/data
              fi

              # download the test files for 'srv4'
              if [[ ${i} == 'srv4' ]] ; then
                     cp ${TMPDATAFOLDER}/1db882c8-8cd6-4df1-941f-ce669bad3458.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/7e480547-fe1a-4eaf-a210-0f3927751a43.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/89120cec-5244-444c-9313-703e4bee72de.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/b74d025e-06d6-43e8-91e1-a862feb03c84.dat ${DATAFOLDER}/${i}/data
                     cp ${TMPDATAFOLDER}/cef4d954-936f-4945-ae49-60ec715b986e.dat ${DATAFOLDER}/${i}/data
                     cp ${PREDEF}/data.zip                                 ${DATAFOLDER}/${i}/data
                     cp ${PREDEF}/large.zip                                ${DATAFOLDER}/${i}/data
              fi
       done

       rm -rf ${TMPDATAFOLDER}

       touch ${DATAFOLDER}/.generated
}

# Launch one daemon in the background and, if it fails, print its log. With -b
# the daemon writes its configuration errors only to the log file and reports
# success or failure through a pipe to the parent, so the test output would
# otherwise end at the trace line for the failed instance with no reason.
launch() {
       local prog=$1 name=$2 log=$3
       if ! ${prog} -b -k fifo -n ${name} -l ${log}.log -s ${log}.pid -c ${name}.cfg; then
              echo "${log} failed to start for ${name}; ${name}/${log}.log follows:"
              cat "${name}/${log}.log"
              exit 1
       fi
}

start(){
       # A previous run killed part way through leaves its servers holding the
       # ports and its instance directories on disk.
       stop
       generate
       set -x
       # Start each instance top-down (meta manager, managers, servers), and
       # its cmsd before its xrootd: xrootd connects to the local cmsd through
       # the admin path at start, so the cmsd must already be there.
       for i in "${servernames[@]}"; do
              launch ${CMSD} ${i} cmsd
              launch ${XROOTD} ${i} xrootd
       done

       sleep 1
}

# Signal only pids that are still alive. A crashed or SIGKILLed instance leaves
# a stale pidfile behind, and killing it blindly under `set -e` would abort the
# script -- which, since this is the fixture *cleanup*, reports the whole
# cluster teardown as failed and leaves the instance directories in place.
# `kill -0` is used for the liveness test because it works the same on procps,
# BusyBox and macOS, where `ps` differs in whether it accepts a pid argument.
#
# After TERM, wait for the daemons to exit before their directories are
# removed: start() calls stop() first, and a daemon still shutting down would
# hold its port. All daemons get TERM first, so their waits overlap. A daemon
# still alive after 10s gets KILL.
stop() {
	local pids=() pid pidfile i alive
	for i in "${servernames[@]}"; do
		for pidfile in "${i}"/cmsd.pid "${i}"/xrootd.pid; do
			test -s "${pidfile}" || continue
			pid="$(cat "${pidfile}")"
			kill -0 "${pid}" 2>/dev/null || continue
			kill -s TERM "${pid}" 2>/dev/null && pids+=("${pid}")
		done
	done
	for _ in $(seq 100); do
		alive=0
		for pid in "${pids[@]}"; do
			kill -0 "${pid}" 2>/dev/null && alive=1
		done
		test ${alive} -eq 1 || break
		sleep 0.1
	done
	for pid in "${pids[@]}"; do
		kill -s KILL "${pid}" 2>/dev/null || :
	done
	for i in "${servernames[@]}"; do
		rm -rf "${i}"
	done
}

insertFileInfo() {
       # modifies metalink data
       for file in ${filenames[@]}; do
              for i in ${PREDEF}/*.meta*; do
                     # update size and hash
                     if grep -q $file $i; then
                            new_size=$(filesize ${TMPDATAFOLDER}/${file})
                            new_hash=$(${CRC32C} < ${TMPDATAFOLDER}/${file} | cut -d' '  -f1)
                            $(formatfiles $i)
                     fi
              done
       done
}

usage() {
       echo $0 start or stop
}

[[ $# == 0 ]] && usage && exit 0

CMD=$1
shift
[[ $(type -t ${CMD}) == "function" ]] || die "unknown command: ${CMD}"
$CMD $@

