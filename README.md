Out-of-tree megaraid_sas driver version 7.x.y.z for GNU/Linux Debian Stretch and Jessie
=======================================================================================

As of Feb. 12th 2018, the megacli_sas driver version available in Debian Stretch (Linux 4.9) does not support Dell's H740p raid controller. Neither do Jessie's (Linux 3.16) and Jessie-backports' (Linux 4.9) ones.

Dell however provides an out-of-tree version of megaraid_sas, version 07.700.52.00, for RHEL 7 and SLES 12.

This repository provides an extract of the RHEL package provided by Dell, ready to be consumed by dkms on Debian systems.

The driver files (sources) stored in this repository can be retrieved again using the `fetch.sh` script (not needed for building/installing).


To install the driver from sources:
-----------------------------------
```
$ dkms add ./dkms.conf
$ dkms build megaraid_sas/07.700.52.00
$ dkms install megaraid_sas/07.700.52.00
```

To build the binary driver deb package:
---------------------------------------
```
$ dkms mkbmdeb megaraid_sas/07.700.52.00
```

To build the dkms deb package:
------------------------------
```
$ ./dkms mkdeb megaraid_sas/07.700.52.00
```
(Needs to use the `./dkms` script provided in this repository to fix a bug with the `mkdeb` command, see https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=832558)


Getting the packages with apt:
------------------------------

The following line can be added to a Debian `sources.list` file, in order to provide the generated deb packages stored in this repository:

```
deb [trusted=yes] https://github.com/npf/megaraid_sas/raw/master/debian /
```

Using the driver in the Debian installer:
-----------------------------------------
The modules directory provides the modules `.ko` files for a direct use, e.g. in the Debian installer.

One can download and `insmod` the driver manually during the installation (hit `Alt-F2` to get a command line).

Or one can use a preseed file containing the following line:
```
d-i partman/early_command string wget "https://github.com/npf/megaraid_sas/raw/master/modules/$(uname -r)/updates/dkms/megaraid_sas.ko" && insmod megaraid_sas.ko
```
Such a preseed file is provided in this repository, and can be used as follows:
1. when in the Debian installer boot menu, press `ESC`
2. then type: `install preseed/url=https://github.com/npf/megaraid_sas/raw/master/preseed.cfg`

Partman should see the disks. If not, check in the console (`Alt-F2`) if the modules is indeed loaded (`lsmod`, `dmesg`). Check for a kernel version mismatch with regard to the files provided here.

Finally, one could setup a PXE netinstall embedding the preseed command to automate everything.

Notes:
------
Watch for updates in the linux-stable branches at kernel.org. Current status:

- https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/tree/drivers/scsi/megaraid/megaraid_sas.h?h=linux-4.14.y
- https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git/tree/drivers/scsi/megaraid/megaraid_sas.h?h=linux-4.9.y
- https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable-rc.git/tree/drivers/scsi/megaraid/megaraid_sas.h?h=linux-4.9.y

Version 07.702.06.00-rc1 in linux-stable 4.14 but version 06.811.02.00-rc1 in linux-stable 4.9 and linux-stable-rc 4.9
