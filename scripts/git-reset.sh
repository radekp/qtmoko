#!/bin/sh
git checkout -f
git submodule init
git submodule update
scripts/git-clean.sh
