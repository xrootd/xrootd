#!/bin/bash
tag=$(git describe --tags --abbrev=0 --exact-match)
echo "RefNames:$tag" > VERSION_INFO
