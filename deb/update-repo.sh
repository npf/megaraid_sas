#!/bin/bash

dpkg-scanpackages -m . | gzip -f9 > Packages.gz
