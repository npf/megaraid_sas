#!/bin/bash
###############################################################################
# This script:
# - fetches the megacli_sas driver (version 7) for H740p from the website of
#   Broadcom or Dell ;
# - extracts sources and prepares/fixes dkms (bogus version).
#
# As of Feb. 12th 2018, this out-of-tree version of megaraid_sas provided by
# Dell is 07.700.52.00, while a more recent version exist in the latest vanilla
# kernel tree (linux 4.14.0 comes with megaraid_sas version 07.702.06.00-rc1)
#
# As of Mar. 21st 2018, Broadcom provides version 7.5-07.705.02.00
#
###############################################################################

set -e

if [ "$1" == "dell" ]; then
# Dell
mkdir -p dell
cd dell
URL=https://downloads.dell.com/FOLDER04586958M/1/UnifiedDriver_7.700.52.00_RHEL7.tar.gz
FILE=${URL##*/}
wget -nc $URL
cat <<EOF > $FILE.sha256sum
5de37701bf6ec2494418c0d92ea68c0cbf29b285e147ee52b1944e851ff0c408  UnifiedDriver_7.700.52.00_RHEL7.tar.gz
EOF
sha256sum -c $FILE.sha256sum
rm $FILE.sha256sum

tar -xvf $FILE
rm $FILE

alien --to-tgz UnifiedDriver_7.700.52.00_RHEL7/megaraid_sas-07.700.52.00_el7.3-2.src.rpm
rm -r UnifiedDriver_7.700.52.00_RHEL7

tar -xvf megaraid_sas-07.700.52.00_el7.3.tgz 
rm megaraid_sas-07.700.52.00_el7.3.tgz megaraid_sas.{conf,files,spec}

tar --strip-components=1 -xvf megaraid_sas-07.700.52.00_el7.3.tar.bz2 
rm megaraid_sas-07.700.52.00_el7.3.tar.bz2

VERSION=07.700.52.00
sed -i -e "s/^\(PACKAGE_VERSION=\).*/\1$VERSION/g" dkms.conf
rm .copyarea.db

elif [ "$1" == "broadcom" ]; then
# Broadcom
mkdir -p broadcom
cd broadcom
URL=https://docs.broadcom.com/docs-and-downloads/raid-controllers/raid-controllers-common-files/MR_LINUX_DRIVER_7.5-07.705.02.00-1.tgz
FILE=${URL##*/}
DIR=${FILE%.tgz}
wget -nc $URL
cat <<EOF > $FILE.sha256sum
20a5e3d2907d054bf143421da8b068f31772a58e0caa348bb050770edddfe6f1  MR_LINUX_DRIVER_7.5-07.705.02.00-1.tgz
EOF
sha256sum -c $FILE.sha256sum
rm $FILE.sha256sum

mkdir -p $DIR
tar -C $DIR -xvf $FILE
rm $FILE

tar --strip-components=1 -xvf $DIR/megaraid_sas-07.705.02.00-src.tar.gz
rm -r $DIR

VERSION=07.705.02.00
sed -i -e "s/^\(PACKAGE_VERSION=\).*/\1$VERSION/g" dkms.conf

else

cat <<EOF
Usage:
  $0 [dell|broadcom]

EOF

fi
