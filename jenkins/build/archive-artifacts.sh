#!/bin/bash

#bail if anything goes wrong
set -e

#clean previous builds
rm -rf libnewrelic/
rm -f libnewrelic*.tar.gz

#create folder structure for tarball
mkdir -p libnewrelic/bin

#move previous build artifacts over 
mv ./php_agent/bin/daemon libnewrelic/bin/newrelic-daemon
mv libnewrelic.a libnewrelic/

#move files from github repository over
mv libnewrelic.h libnewrelic/
mv GUIDE.md libnewrelic/
mv LICENSE.txt libnewrelic/

#archive and gzip teh project
tar -cvf libnewrelic.${VERSION}.tar libnewrelic 
gzip libnewrelic.${VERSION}.tar
