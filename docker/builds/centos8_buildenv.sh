#!/bin/bash

docker build -t xrootd/centos8/build - < DockerfileCentos8
docker run -it --rm xrootd/centos8/build
