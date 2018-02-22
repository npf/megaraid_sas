#!/bin/bash

tmpdir=$(mktemp -d)
for f in ../deb/megaraid-sas-modules-*; do
  dpkg-deb -x $f $tmpdir
done
cp -av $tmpdir/lib/modules/* .
rm -rf $tmpdir

