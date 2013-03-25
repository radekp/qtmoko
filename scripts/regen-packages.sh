#!/bin/sh
dpkg-scanpackages . /dev/null | gzip -9c > Packages.gz
dpkg-scanpackages . /dev/null > Packages
apt-ftparchive release . > Release
gpg -abs -o Release.gpg Release
