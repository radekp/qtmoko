#!/bin/sh
git reset HEAD --hard
git submodule init
git submodule update
git submodule foreach "git reset HEAD --hard"
scripts/git-clean.sh
