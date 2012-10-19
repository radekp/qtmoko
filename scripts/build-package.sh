#!/bin/sh
scripts/git-reset.sh
debian/rules get-orig-source
dpkg-buildpackage
