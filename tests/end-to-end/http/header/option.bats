#!/usr/bin/env bash

# the xrdcp option table, which the header argument never reaches

load ../../setup/server.bash

export XRD_PLUGINCONFDIR="$SETUP_DIR/client-plugins"

#
# the option takes a mandatory argument
#

@test "-H with no argument fails" {
	run ! xrdcp -H
}

@test "--header with no argument fails" {
	run ! xrdcp --header
}

#
# options -H took over
#

@test "--license is still available after -H was reassigned" {
	run xrdcp --license
	assert_output --partial 'GNU Lesser General Public License'
}
