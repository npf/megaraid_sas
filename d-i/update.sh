#!/bin/bash

URL="https://github.com/npf/megaraid_sas/raw/master"
for path in ../deb/*amd64.deb; do
  file=${path##*/}
  t=${file#megaraid-sas-modules-}
  kernel_version=${t%%_*}
  # Hope ls sorts files with the latest version lastest.
  echo "wget $URL/deb/$file && udpkg -i $file" > $kernel_version
done

