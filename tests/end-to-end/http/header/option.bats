#!/usr/bin/env bash

# the xrdcp option table, which the header argument never reaches

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"

#
# the option takes a mandatory argument
#

# With musl, xrdcp crashed here, which run ! takes as a failure too, thus
# check the message (fixed in XrdApps)
@test "-H with no argument fails" {
	run ! xrdcp -H
	assert_output --partial "'-H' argument missing"
}

@test "-H with no argument after an operand fails" {
	run ! xrdcp file -H
	assert_output --partial "'-H' argument missing"
}

@test "--header with no argument fails" {
	run ! xrdcp --header
	assert_output --partial "'--header' argument missing"
}

#
# options -H took over
#

@test "--license is still available after -H was reassigned" {
	run xrdcp --license
	assert_output --partial 'GNU Lesser General Public License'
}
