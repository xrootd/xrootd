#!/usr/bin/env bash

# the xrdcp option table, which the header argument never reaches

bats_require_minimum_version 1.5.0

bats_load_library 'bats-support'
bats_load_library 'bats-assert'

load ../../helper/common.bash

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
	assert_output --partial 'Copyright'
}
