#!/usr/bin/env bash

# cconfig, which prints the directives of a configuration file which apply to
# a program, an instance and a host: after the substitution of variables, the
# selection of if blocks, and the continue directives.

load ../setup/server.bash

setup() {
    cd "$BATS_TEST_TMPDIR"
    mkdir more
    cat > main.cfg <<'CFG'
all.sitename main
set n = 5
xrd.port 1094
if exec cmsd
xrd.port 2094
fi
if named alpha
all.role manager
fi
oss.localroot /data$n
continue more.cfg
CFG
    echo 'xrootd.trace all' > more.cfg
    echo 'ofs.trace all' > more/a.cfg
    echo 'oss.trace all' > more/b.txt
}

@test "cconfig prints the directives of the configuration file" {
    run -0 cconfig -c main.cfg
    assert_line --regexp '^all\.sitename main *$'
}

@test "cconfig substitutes the variables which set defines" {
    run -0 cconfig -c main.cfg
    assert_line --regexp '^oss\.localroot /data5 *$'
}

@test "cconfig skips the directives for another program" {
    run -0 cconfig -c main.cfg
    refute_line --regexp '^xrd\.port 2094'
}

@test "cconfig with -x prints the directives for that program" {
    run -0 cconfig -c main.cfg -x cmsd
    assert_line --regexp '^xrd\.port 2094'
}

@test "cconfig skips the directives of a named instance" {
    run -0 cconfig -c main.cfg
    refute_line --regexp '^all\.role manager'
}

@test "cconfig with -n prints the directives of that instance" {
    run -0 cconfig -c main.cfg -n alpha
    assert_line --regexp '^all\.role manager'
}

@test "cconfig with a directive prints only the directives which match" {
    run -0 cconfig -c main.cfg 'xrd.*'
    refute_line --regexp '^all\.sitename'
}

@test "cconfig follows a continue directive to a file" {
    run -0 cconfig -c main.cfg
    assert_line --regexp '^xrootd\.trace all'
}

@test "cconfig follows a continue directive to the matching files of a directory" {
    sed -i.orig 's/^continue more.cfg$/continue more *.cfg/' main.cfg
    run -0 cconfig -c main.cfg
    assert_line --regexp '^ofs\.trace all'
}

@test "a continue directive to a directory skips the files which do not match" {
    sed -i.orig 's/^continue more.cfg$/continue more *.cfg/' main.cfg
    run -0 cconfig -c main.cfg
    refute_line --regexp '^oss\.trace all'
}

@test "cconfig fails for a configuration file which does not exist" {
    run -4 cconfig -c missing.cfg
}
