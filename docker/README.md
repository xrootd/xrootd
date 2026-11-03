# Building and testing XRootD in a container

The script `xrd-docker` and the files `Dockerfile.<OS>` in the `docker/`
directory build XRootD in a container of another distribution, and run the
test suite as part of the build. The RPM-based images build the packages with
`rpmbuild`, which runs `ctest` in the `%check` section of the spec file. The
Alpine image runs `ctest -S test.cmake`, installs the build, and runs
`tests/post-install.sh` and `tests/check-headers.sh`. A test which fails
stops the build of the image. The container that results can then be started
for debugging if needed.

## Contents

- [Dependencies](#dependencies)
- [Create an XRootD tarball to build in the container](#create-an-xrootd-tarball-to-build-in-the-container)
- [Build the container image](#build-the-container-image)
  - [Switching between `docker` and `podman` if both are installed](#switching-between-docker-and-podman-if-both-are-installed)
- [Appendix](#appendix)
  - [Setting up `podman`](#setting-up-podman)

## Dependencies

On RHEL 8 and 9, and their derivatives, install `podman` with
`dnf groupinstall 'Container Management'`. On Debian and Ubuntu, install it
with `apt install podman`. To make sure that `podman` works, start a busybox
image:

```sh
podman run --rm -it busybox
```

## Create an XRootD tarball to build in the container

The examples below run `xrd-docker` from the `docker/` directory. From the
root of the repository, run `./docker/xrd-docker` instead. The script changes
to its own directory before it runs a command, so the files go into `docker/`
in both cases.

First, make a tarball with the version of XRootD to build. The command
`xrd-docker package` makes the tarball `xrootd.tar.gz` in the `docker/`
directory, from `HEAD` of the current branch by default. Give a version to
use another commit. For example, for the release v6.3.0, run:

```sh
$ ./xrd-docker package v6.3.0
```

The command makes a standard tarball with `git archive`. The `VERSION` file in
the tarball contains the expanded version, which the spec file uses to find the
version of XRootD. A source RPM made from such a tarball must be built with
`rpmbuild --with git`, as the CI builds and the files `Dockerfile.<OS>` do.

Git gets the version from `git describe`, so the clone must contain the
release tags and the history back to the last tag. Otherwise, the version is
empty. In a shallow clone, get the history and the tags first with
`git fetch --unshallow --tags`. In a full clone without tags, use
`git fetch --tags`.

## Build the container image

Then, build the container image for the operating system. The command is:

```sh
$ ./xrd-docker build <OS>
```

`<OS>` is one of `alma9` (default), `alma8`, `fedora`, `alpine`, `debian`, or
`ubuntu`. The command uses the file `docker/Dockerfile.<OS>`. To add a custom
setup, add a file with the same name pattern. The command names the image
`xrootd`, and also tags it `xrootd:<OS>`, so that images for several operating
systems can exist together. The command `build` makes the tarball first when
it does not exist.

`xrd-docker buildx <OS> <ARCH>` builds an image for another architecture:
`amd64`, `aarch64`, `ppc64le` or `s390x`. The default OS is `fedora`. Run
`xrd-docker qemu` one time first, to register QEMU for the other
architectures if you do not have a working QEMU in your system.
`xrd-docker clean` removes the tarball.

### Switching between `docker` and `podman` if both are installed

`xrd-docker` uses `podman` if it is installed, and `docker` otherwise.
To use another program, set the environment variable `DOCKER`:

```sh
$ export DOCKER=$(command -v docker)
$ ./xrd-docker build # uses docker from now on...
```

`xrd-docker qemu` may not work with `podman`. In that case, you need to
re-register the [`binfmt_misc`](https://docs.kernel.org/admin-guide/binfmt-misc.html)
handlers and try again with `docker` instead. An easy way to clear the
`binfmt_misc` handlers is to reboot the machine. If you prefer to set this
up with a local installation of [QEMU](https://www.qemu.org/docs/master/user/main.html),
check also the documentation for the
[`systemd-binfmt.service`](https://www.freedesktop.org/software/systemd/man/latest/systemd-binfmt.service.html)
from systemd.

## Appendix

### Setting up `podman`

Unlike `docker`, `podman` may not work immediately after the installation. If
it does not work, make sure that your user has subordinate UIDs and GIDs, for
example with these two commands:

```sh
$ sudo usermod --add-subuids 1000000-1000999999 $(id -un)
$ sudo usermod --add-subgids 1000000-1000999999 $(id -un)
```

On a host with more than one user, each user must have a different range.
Ranges which overlap let one user get access to the containers of another
user. Before you add a range, examine `/etc/subuid` and `/etc/subgid` for the
ranges that are in use, and select a range which does not overlap them.

If you used `podman` before the change, the old ID mappings stay in effect.
Stop all your containers, then run `podman system migrate` to apply the new
mappings.

Also make sure that `/etc/containers/registries.conf` configures the
container registries. Usually, you can rename
`/etc/containers/registries.conf.example` to get a usable configuration.

You can also try container runtimes other than the default. The documentation
of `podman` is at https://podman.io/docs.
