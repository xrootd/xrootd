#!/bin/bash

docker build -t xrootd/centos7/build - < DockerfileCentos7
docker run -it --rm xrootd/centos7/build
