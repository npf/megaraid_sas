#!/bin/bash

dpkg-scanpackages . | gzip -f9 > Packages.gz
