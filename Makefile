# Convenience wrapper around the CMake build so the whole test suite — the
# GoogleTest unit/integration tests and the BATS end-to-end tests — can be
# built and run with a single command:
#
#     make test
#
# CMake remains the real build system; this file only forwards to it. Any
# variable below can be overridden on the command line, e.g.:
#
#     make test BUILD_DIR=out BUILD_TYPE=RelWithDebInfo
#     make test CTEST_ARGS="-R end-to-end --parallel 4"

BUILD_DIR  ?= build
BUILD_TYPE ?= Debug
CMAKE      ?= cmake
CTEST      ?= ctest
CMAKE_ARGS ?=
CTEST_ARGS ?=

CONFIGURE_ARGS := -DENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)

.PHONY: all configure build test clean

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CONFIGURE_ARGS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel

test: build
	cd $(BUILD_DIR) && $(CTEST) --output-on-failure $(CTEST_ARGS)

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
