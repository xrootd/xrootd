
#
# The XrdOssTests is a wrapper OSS that injects specific behaviors
# (typically, errors) into the filesystem for the purpose of allowing
# the testing of failures
#

add_library( XrdOssTests MODULE xrdoss_tests.cc )
target_link_libraries( XrdOssTests XrdServer )

set_target_properties( XrdOssTests PROPERTIES OUTPUT_NAME "XrdOssTests-${PLUGIN_VERSION}" SUFFIX ".so" )

