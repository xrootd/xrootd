#!/usr/bin/env bash

load "$(dirname $(realpath ${BASH_SOURCE[0]}))/helper/tls.bash"

setup_suite() {
    generate_tls
}
