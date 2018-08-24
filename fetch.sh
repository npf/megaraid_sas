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

usage() {
  cat <<EOF 1>&2
Usage: $0 <dell|broadcom|URL> [version]

Arguments:
  dell      use the last known Dell driver URL
  broadcom  use the last known Broadcom (upstream) driver URL
  URL       provide URL yourself
  version   version if not retievable from URL (optional)

EOF
exit 0
}

die() {
  echo "Error: $1" 1>&2
  exit 1
}

if [ -z "$1" -o "$1" == "--help" -o "$1" == "-h" ]; then
  usage
elif [ "$1" == "dell" ]; then
  # Search driver from Dell's support/driver pages
  #URL=https://downloads.dell.com/FOLDER04586958M/1/UnifiedDriver_7.700.52.00_RHEL7.tar.gz
  URL=https://downloads.dell.com/FOLDER04716643M/1/UnifiedDriver_7.703.06.00_RHEL7.tar.gz
elif [ "$1" == "broadcom" ]; then
  # Search from https://www.broadcom.com/support/download-search
  #URL=https://docs.broadcom.com/docs-and-downloads/raid-controllers/raid-controllers-common-files/MR_LINUX_DRIVER_7.5-07.705.02.00-1.tgz
  URL=https://docs.broadcom.com/docs-and-downloads/raid-controllers/raid-controllers-common-files/MR_LINUX_DRIVER_7.6-07.706.03.00-1.tgz
else 
  URL=$1
fi

if [ -n "$2" ]; then
  VERSION=$2
elif [[ $URL =~ ([[:digit:]]?)([[:digit:]]\.[[:digit:]]+\.[[:digit:]]+\.[[:digit:]]+) ]]; then
  VERSION="${BASH_REMATCH[1]:-0}${BASH_REMATCH[2]}"
else
  echo "Cannot retrive VERSION" 1>&2
  usage
  exit 1
fi

echo "URL=$URL"
echo "VERSION=$VERSION"

if [[ $URL =~ ^https://.+\.dell\.com/ ]]; then
  DIR=dell-$VERSION
  mkdir -p $DIR
  echo $URL > $DIR/SOURCE_URL
  cd $DIR
  FILE=${URL##*/}
  wget -nc $URL
  SRPM=$(tar -tf $FILE | grep .src.rpm)
  [ -z "$SRPM" ] && die "Cannot retrieve SRPM filename"
  [[ $SRPM =~ ^/ ]] && die "SRPM file path is not expected not begins with /"
  echo "SRPM=$SRPM"
  tar -xvf $FILE $SRPM
  rm $FILE
  
  echo "Convert to tgz (alien)..."
  alien --to-tgz $SRPM
  rm -rv ${SRPM%%/*}

  BASENAME=${SRPM##*/}  
  ALIEN_TGZ=${BASENAME%-*.src.rpm}.tgz
  DRV_ARCHIVE=$(tar -tf $ALIEN_TGZ | grep $VERSION)
  [ -z "$DRV_ARCHIVE" ] && die "Cannot retrieve DRV_ARCHIVE filename"
  [[ $DRV_ARCHIVE =~ ^/ ]] && die "DRV_ARCHIVE file path is not expected not begins with /"
  echo "DRV_ARCHIVE=$DRV_ARCHIVE"
  tar -xvf $ALIEN_TGZ
  rm $ALIEN_TGZ megaraid_sas.{conf,files,spec}
  
  echo "Extract driver files from archive..."
  tar --strip-components=1 -xvf $DRV_ARCHIVE
  rm $DRV_ARCHIVE

  echo "Fix files..."
  sed -i -e "s/^\(PACKAGE_VERSION=\).*/\1$VERSION/g" dkms.conf
  rm -f .copyarea.db
elif [[ $URL =~ ^https://.+\.broadcom\.com/ ]]; then
  DIR=broadcom-$VERSION
  mkdir -p $DIR
  echo $URL > $DIR/SOURCE_URL
  mkdir -p $DIR
  cd $DIR
  FILE=${URL##*/}
  TMPDIR=${FILE%.tgz}
  wget -nc $URL
  mkdir -p $TMPDIR
  echo "Extract driver source archive from Broadcom driver bundle..."
  tar -C $TMPDIR -xvf $FILE megaraid_sas-$VERSION-src.tar.gz
  rm $FILE
  
  echo "Extract driver files from driver source archive..."
  tar --strip-components=1 -xvf $TMPDIR/megaraid_sas-$VERSION-src.tar.gz
  rm -r $TMPDIR
  
  echo "Fix files..."
  sed -i -e "s/^\(PACKAGE_VERSION=\).*/\1$VERSION/g" dkms.conf
fi
