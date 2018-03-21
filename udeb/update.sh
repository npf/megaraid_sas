#!/bin/bash

for path in ../deb/*amd64.deb; do
  file=${path##*/}
  link=${file%%_*}.deb
  # Hope ls sorts files correctly here ;-)
  ln -sf $path $link
done
ls -l *.deb
