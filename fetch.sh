#!/bin/bash
###############################################################################
# This script:
# - fetches the megacli_sas driver (version 7) for H740p from Dell's website ;
# - extracts sources and prepares/fixes dkms (bogus version).
#
# As of Feb. 12th 2018, this out-of-tree version of megaraid_sas provided by
# Dell is 07.700.52.00, while a more recent version exist in the latest vanilla
# kernel tree (linux 4.14.0 comes with megaraid_sas version 07.702.06.00-rc1)
###############################################################################

set -e

wget -nc https://downloads.dell.com/FOLDER04586958M/1/UnifiedDriver_7.700.52.00_RHEL7.tar.gz
cat <<EOF > UnifiedDriver_7.700.52.00_RHEL7.tar.gz.sha256sum
5de37701bf6ec2494418c0d92ea68c0cbf29b285e147ee52b1944e851ff0c408  UnifiedDriver_7.700.52.00_RHEL7.tar.gz
EOF
sha256sum -c UnifiedDriver_7.700.52.00_RHEL7.tar.gz.sha256sum
rm UnifiedDriver_7.700.52.00_RHEL7.tar.gz.sha256sum

tar -xvf UnifiedDriver_7.700.52.00_RHEL7.tar.gz 
rm UnifiedDriver_7.700.52.00_RHEL7.tar.gz

alien --to-tgz UnifiedDriver_7.700.52.00_RHEL7/megaraid_sas-07.700.52.00_el7.3-2.src.rpm
rm -r UnifiedDriver_7.700.52.00_RHEL7

tar -xvf megaraid_sas-07.700.52.00_el7.3.tgz 
rm megaraid_sas-07.700.52.00_el7.3.tgz megaraid_sas.{conf,files,spec}

tar --strip-components=1 -xvf megaraid_sas-07.700.52.00_el7.3.tar.bz2 
rm megaraid_sas-07.700.52.00_el7.3.tar.bz2

VERSION=07.700.52.00
sed -i -e "s/^\(PACKAGE_VERSION=\).*/\1$VERSION/g" dkms.conf
rm .copyarea.db
