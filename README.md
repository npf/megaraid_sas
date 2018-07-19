Out-of-tree megaraid_sas driver version 7.x.y.z for GNU/Linux Debian Stretch and Jessie
=======================================================================================

As of Feb. 12th 2018, the megacli_sas driver version available in Debian Stretch (Linux 4.9) does not support Dell's H740p raid controller. Neither do Jessie's (Linux 3.16) and Jessie-backports' (Linux 4.9) ones.

Dell and Broadcom however provide out-of-tree version of megaraid_sas for some distributions but not Debian.

This repository provides packages ready to be consumed by dkms on Debian systems, as well as binary modules packages for the usual kernel versions.
 
The driver files (sources) stored in this repository (directory dell and broadcom) can be retrieved again using the `fetch.sh` script (not needed for building/installing).

First choose the broadcom or the dell version of driver
-------------------------------------------------------

`$ cd broadcom_07.XXX.XX.XX` or `cd dell_07.XXX.XX.XX`

To install the driver from sources:
-----------------------------------
```
$ dkms add ./dkms.conf
$ dkms build megaraid_sas/07.XXX.XX.XX
$ dkms install megaraid_sas/07.XXX.XX.XX
```

To build the binary driver deb package:
---------------------------------------
```
$ dkms mkbmdeb megaraid_sas/07.XXX.XX.XX
```

To build the dkms deb package:
------------------------------
```
$ ../dkms mkdeb megaraid_sas/07.XXX.XX.XX
```
(Needs to use the `dkms` script provided in this repository to fix a bug with the `mkdeb` command, see https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=832558)

Generated deb files are stored in `/var/lib/dkms/megaraid_sas/07.XXX.XX.XX.XX/deb`. Files in the `deb` directory of this repository were copied from there after build.

Getting the packages with apt:
------------------------------

The following line can be added to a Debian `sources.list` file, in order to provide the generated deb packages stored in this repository:

```
deb [trusted=yes] https://github.com/npf/megaraid_sas/raw/master/deb /
```

Using the driver in the Debian installer:
-----------------------------------------
One can use a preseed file in order to have the last drivers provided here used in the Debian installer, making use of the `d-i partman/early_command` directive.

Such a preseed file is provided in this repository, and can be used as follows:
1. when in the Debian installer boot menu, press `ESC` ;
2. then type: `install preseed/url=https://github.com/npf/megaraid_sas/raw/master/preseed.cfg`.

Partman should see the disks. If not, check in the console (`Alt-F2`) if the modules is indeed loaded (`lsmod`, `dmesg`). Check for a kernel version mismatch with regard to the files provided here.

One could also setup a PXE netinstall embedding the preseed command to automate everything.

Notes:
------
Watch for updates in the linux-stable branches at kernel.org. Current status:

- https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/tree/drivers/scsi/megaraid/megaraid_sas.h?h=linux-4.14.y
- https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/tree/drivers/scsi/megaraid/megaraid_sas.h?h=linux-4.9.y
- https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable-rc.git/tree/drivers/scsi/megaraid/megaraid_sas.h?h=linux-4.9.y

Version 07.702.06.00-rc1 in linux-stable 4.14 but version 06.811.02.00-rc1 in linux-stable 4.9 and linux-stable-rc 4.9
