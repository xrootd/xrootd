#!/bin/sh -xe

# This script starts docker and systemd (if el7)

# Run tests in Container
if [ "${OS_VERSION}" = "7" ]; then

    docker run --privileged --detach --tty --interactive --env "container=docker" \
           --volume /sys/fs/cgroup:/sys/fs/cgroup \
           --volume `pwd`:/xrootd-scitokens:rw  \
           centos:centos${OS_VERSION} \
           /usr/sbin/init

    DOCKER_CONTAINER_ID=$(docker ps | grep centos | awk '{print $1}')
    docker logs $DOCKER_CONTAINER_ID
    docker exec --tty --interactive $DOCKER_CONTAINER_ID \
           /bin/bash -xec "bash -xe /xrootd-scitokens/test/test_inside_docker.sh ${OS_VERSION};
           echo -ne \"------\nEND XROOTD-SCITOKENS TESTS\n\";"

    docker ps -a
    #docker stop $DOCKER_CONTAINER_ID
    #docker rm -v $DOCKER_CONTAINER_ID

fi
