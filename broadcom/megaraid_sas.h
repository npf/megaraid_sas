/*
 *  Linux MegaRAID driver for SAS based RAID controllers
 *
 *  Copyright (c) 2003-2017  LSI Corporation.
 *  Copyright (c) 2003-2017  Avago Technologies.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  FILE: megaraid_sas.h
 *
 *  Authors: Avago Technologies
 *
 *  Send feedback to: <megaraidlinux.pdl@avagotech.com>
 *
 *  ATTN: Linuxraid
 */

#ifndef LSI_MEGARAID_SAS_H
#define LSI_MEGARAID_SAS_H

/*
 * MegaRAID SAS Driver meta data
 */
#define MEGASAS_VERSION				"07.705.02.00"
#define MEGASAS_RELDATE				"Nov 24, 2017"
#define MEGASAS_EXT_VERSION			"Nov 24, 17:00:00 PDT 2017"

/*
 * Device IDs
 */
#define	PCI_DEVICE_ID_LSI_SAS1078R		0x0060
#define	PCI_DEVICE_ID_LSI_SAS1078DE		0x007C
#define	PCI_DEVICE_ID_LSI_VERDE_ZCR		0x0413
#define	PCI_DEVICE_ID_LSI_SAS1078GEN2		0x0078
#define	PCI_DEVICE_ID_LSI_SAS0079GEN2		0x0079
#define	PCI_DEVICE_ID_LSI_SAS0073SKINNY		0x0073
#define	PCI_DEVICE_ID_LSI_SAS0071SKINNY		0x0071
#define	PCI_DEVICE_ID_LSI_FUSION		0x005b
#define PCI_DEVICE_ID_LSI_PLASMA		0x002f
#define PCI_DEVICE_ID_LSI_INVADER		0x005d
#define PCI_DEVICE_ID_LSI_FURY			0x005f
#define PCI_DEVICE_ID_LSI_INTRUDER		0x00ce
#define PCI_DEVICE_ID_LSI_INTRUDER_24		0x00cf
#define PCI_DEVICE_ID_LSI_CUTLASS_52		0x0052
#define PCI_DEVICE_ID_LSI_CUTLASS_53		0x0053
#define PCI_DEVICE_ID_LSI_VENTURA		0x0014
#define PCI_DEVICE_ID_LSI_CRUSADER		0x0015
#define PCI_DEVICE_ID_LSI_HARPOON		0x0016
#define PCI_DEVICE_ID_LSI_TOMCAT		0x0017
#define PCI_DEVICE_ID_LSI_VENTURA_4PORT		0x001B
#define PCI_DEVICE_ID_LSI_CRUSADER_4PORT	0x001C

/*                      
 * Intel HBA SSDIDs         
 */ 
#define MEGARAID_INTEL_RS3DC080_SSDID 		0x9360    
#define MEGARAID_INTEL_RS3DC040_SSDID 		0x9362    
#define MEGARAID_INTEL_RS3SC008_SSDID 		0x9380    
#define MEGARAID_INTEL_RS3MC044_SSDID 		0x9381    
#define MEGARAID_INTEL_RS3WC080_SSDID 		0x9341    
#define MEGARAID_INTEL_RS3WC040_SSDID 		0x9343    
#define MEGARAID_INTEL_RMS3BC160_SSDID 		0x352B

/*
 * Intruder HBA SSDIDs
 */
#define MEGARAID_INTRUDER_SSDID1 		0x9371
#define MEGARAID_INTRUDER_SSDID2		0x9390
#define MEGARAID_INTRUDER_SSDID3 		0x9370



/*
 * Intel HBA branding
 */
#define MEGARAID_INTEL_RS3DC080_BRANDING	\
	"Intel(R) RAID Controller RS3DC080"
#define MEGARAID_INTEL_RS3DC040_BRANDING	\
	"Intel(R) RAID Controller RS3DC040"
#define MEGARAID_INTEL_RS3SC008_BRANDING	\
	"Intel(R) RAID Controller RS3SC008"
#define MEGARAID_INTEL_RS3MC044_BRANDING	\
	"Intel(R) RAID Controller RS3MC044"
#define MEGARAID_INTEL_RS3WC080_BRANDING	\
	"Intel(R) RAID Controller RS3WC080"
#define MEGARAID_INTEL_RS3WC040_BRANDING	\
	"Intel(R) RAID Controller RS3WC040"
#define MEGARAID_INTEL_RMS3BC160_BRANDING	\
	"Intel(R) Integrated RAID Module RMS3BC160"


/*
 * =====================================
 * MegaRAID SAS MFI firmware definitions
 * =====================================
 */

/*
 * MFI stands for  MegaRAID SAS FW Interface. This is just a moniker for 
 * protocol between the software and firmware. Commands are issued using
 * "message frames"
 */

/*
 * FW posts its state in upper 4 bits of outbound_msg_0 register
 */
#define MFI_STATE_MASK				0xF0000000
#define MFI_STATE_UNDEFINED			0x00000000
#define MFI_STATE_BB_INIT			0x10000000
#define MFI_STATE_FW_INIT			0x40000000
#define MFI_STATE_WAIT_HANDSHAKE		0x60000000
#define MFI_STATE_FW_INIT_2			0x70000000
#define MFI_STATE_DEVICE_SCAN			0x80000000
#define MFI_STATE_BOOT_MESSAGE_PENDING		0x90000000
#define MFI_STATE_FLUSH_CACHE			0xA0000000
#define MFI_STATE_READY				0xB0000000
#define MFI_STATE_OPERATIONAL			0xC0000000
#define MFI_STATE_FAULT				0xF0000000
#define MFI_STATE_FORCE_OCR			0x00000080
#define MFI_STATE_DMADONE			0x00000008
#define MFI_STATE_CRASH_DUMP_DONE		0x00000004
#define MFI_RESET_REQUIRED			0x00000001
#define MFI_RESET_ADAPTER			0x00000002
#define MEGAMFI_FRAME_SIZE			64

/*
 * During FW init, clear pending cmds & reset state using inbound_msg_0
 *
 * ABORT	: Abort all pending cmds
 * READY	: Move from OPERATIONAL to READY state; discard queue info
 * MFIMODE	: Discard (possible) low MFA posted in 64-bit mode (??)
 * CLR_HANDSHAKE: FW is waiting for HANDSHAKE from BIOS or Driver
 * HOTPLUG	: Resume from Hotplug
 * MFI_STOP_ADP	: Send signal to FW to stop processing
 */

#define WRITE_SEQUENCE_OFFSET		(0x0000000FC) // I20
#define HOST_DIAGNOSTIC_OFFSET		(0x000000F8)  // I20
#define DIAG_WRITE_ENABLE			(0x00000080)
#define DIAG_RESET_ADAPTER			(0x00000004)

#define MFI_ADP_RESET				0x00000040
#define MFI_INIT_ABORT				0x00000001
#define MFI_INIT_READY				0x00000002
#define MFI_INIT_MFIMODE			0x00000004
#define MFI_INIT_CLEAR_HANDSHAKE		0x00000008
#define MFI_INIT_HOTPLUG			0x00000010
#define MFI_STOP_ADP				0x00000020
#define MFI_RESET_FLAGS				MFI_INIT_READY| \
						MFI_INIT_MFIMODE| \
						MFI_INIT_ABORT

/* MsgFlags */
#define MPI2_IOCINIT_MSGFLAG_RDPQ_ARRAY_MODE    (0x01)


/*
 * MFI frame flags
 */
#define MFI_FRAME_POST_IN_REPLY_QUEUE		0x0000
#define MFI_FRAME_DONT_POST_IN_REPLY_QUEUE	0x0001
#define MFI_FRAME_SGL32				0x0000
#define MFI_FRAME_SGL64				0x0002
#define MFI_FRAME_SENSE32			0x0000
#define MFI_FRAME_SENSE64			0x0004
#define MFI_FRAME_DIR_NONE			0x0000
#define MFI_FRAME_DIR_WRITE			0x0008
#define MFI_FRAME_DIR_READ			0x0010
#define MFI_FRAME_DIR_BOTH			0x0018
#define MFI_FRAME_IEEE                          0x0020

/* Driver internal */
#define DRV_DCMD_POLLED_MODE		0x1
#define DRV_DCMD_SKIP_REFIRE		0x2
#define	BLK_TAG_DEBUG			0
#define	BLK_TAG_REFCOUNT		0

/*
 * Definition for cmd_status
 */
#define MFI_CMD_STATUS_POLL_MODE		0xFF

/*
 * MFI command opcodes
 */
typedef enum _MFI_CMD_OP {
	MFI_CMD_INIT		= 0x0,
	MFI_CMD_LD_READ		= 0x1,
	MFI_CMD_LD_WRITE	= 0x2,
	MFI_CMD_LD_SCSI_IO	= 0x3,
	MFI_CMD_PD_SCSI_IO	= 0x4,
	MFI_CMD_DCMD		= 0x5,
	MFI_CMD_ABORT		= 0x6,
	MFI_CMD_SMP		= 0x7,
	MFI_CMD_STP		= 0x8,
	MFI_CMD_NVME		= 0x9,
	MFI_CMD_OP_COUNT,
	MFI_CMD_INVALID		= 0xff
} MFI_CMD_OP;

#define MR_DCMD_PD_GET_INFO					0x02020000
#define MR_DCMD_CTRL_SET_CRASH_DUMP_PARAMS 	0x01190100
#define MR_DRIVER_SET_APP_CRASHDUMP_MODE 	(0xF0010000 | 0x0600)
#define MR_DCMD_CTRL_GET_INFO			0x01010000
#define MR_DCMD_LD_GET_LIST			0x03010000
#define MR_DCMD_LD_LIST_QUERY			0x03010100

#define MR_DCMD_CTRL_CACHE_FLUSH		0x01101000
#define MR_FLUSH_CTRL_CACHE			0x01
#define MR_FLUSH_DISK_CACHE			0x02

#define MR_DCMD_CTRL_SHUTDOWN			0x01050000
#define MR_DCMD_HIBERNATE_SHUTDOWN		0x01060000
#define MR_ENABLE_DRIVE_SPINDOWN		0x01

#define MR_DCMD_CTRL_EVENT_GET_INFO		0x01040100
#define MR_DCMD_CTRL_EVENT_GET			0x01040300
#define MR_DCMD_CTRL_EVENT_WAIT			0x01040500
#define MR_DCMD_LD_GET_PROPERTIES		0x03030000

#define MR_DCMD_CLUSTER				0x08000000
#define MR_DCMD_CLUSTER_RESET_ALL		0x08010100
#define MR_DCMD_CLUSTER_RESET_LD		0x08010200
#define MR_DCMD_PD_LIST_QUERY                   0x02010100

#define MR_DCMD_CTRL_MISC_CPX                   0x0100e200
#define MR_DCMD_CTRL_MISC_CPX_INIT_DATA_GET     0x0100e201
#define MR_DCMD_CTRL_MISC_CPX_QUEUE_DATA        0x0100e202
#define MR_DCMD_CTRL_MISC_CPX_UNREGISTER        0x0100e203
#define MAX_MR_ROW_SIZE                         32
#define MR_CPX_DIR_WRITE                        1
#define MR_CPX_DIR_READ                         0
#define MR_CPX_VERSION                          1

#define MR_DCMD_CTRL_IO_METRICS_GET             0x01170200   // get IO metrics
#define MR_DCMD_CTRL_BIOS_DATA_GET              0x010c0100
#define MR_DCMD_PD_MFI_TASK_MGMT                0x0200e100

#define MR_EVT_CFG_CLEARED			0x0004

#define MR_EVT_LD_STATE_CHANGE			0x0051
#define MR_EVT_PD_INSERTED			0x005b
#define MR_EVT_PD_REMOVED			0x0070
#define MR_EVT_LD_CREATED			0x008a
#define MR_EVT_LD_DELETED			0x008b
#define MR_EVT_FOREIGN_CFG_IMPORTED		0x00db
#define MR_EVT_LD_OFFLINE			0x00fc
#define MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED	0x0152
#define MR_EVT_CTRL_PERF_COLLECTION		0x017e
#define MR_EVT_CTRL_PROP_CHANGED		0x012f

#define MAX_LOGICAL_DRIVES                      64
#define MAX_LOGICAL_DRIVES_EXT                  256

#define MEGASAS_MAX_PD_CHANNELS			2
#define MEGASAS_MAX_LD_CHANNELS			2
#define MEGASAS_MAX_CHANNELS			(MEGASAS_MAX_PD_CHANNELS + \
						MEGASAS_MAX_LD_CHANNELS)
#define MEGASAS_MAX_DEV_PER_CHANNEL		128
#define MEGASAS_DEFAULT_INIT_ID			-1
#define MEGASAS_MAX_LUN				8
#define MEGASAS_DEFAULT_CMD_PER_LUN		256
#define MEGASAS_MAX_PD                          (MEGASAS_MAX_PD_CHANNELS * \
						MEGASAS_MAX_DEV_PER_CHANNEL)
#define MEGASAS_MAX_LD_IDS			(MEGASAS_MAX_LD_CHANNELS * \
							MEGASAS_MAX_DEV_PER_CHANNEL)

#define MEGASAS_INVALID_TARGET_ID               0xFFFF
#define MEGASAS_KDUMP_QUEUE_DEPTH               100

#define MEGASAS_CLUSTER_ID_SIZE				16
#define MR_LARGE_IO_MIN_SIZE			(32 * 1024)
#define MR_R1_LDIO_PIGGYBACK_DEFAULT		4

typedef enum _MR_SCSI_CMD_TYPE {
        READ_WRITE_LDIO = 0,
        NON_READ_WRITE_LDIO = 1,
        READ_WRITE_SYSPDIO = 2,
        NON_READ_WRITE_SYSPDIO = 3,
} MR_SCSI_CMD_TYPE;

typedef enum _DCMD_TIMEOUT_ACTION {
        INITIATE_OCR = 0,
        KILL_ADAPTER = 1,
        IGNORE_TIMEOUT = 2,
} DCMD_TIMEOUT_ACTION;

typedef enum _FW_BOOT_CONTEXT {
        PROBE_CONTEXT = 0,
        OCR_CONTEXT = 1,
} FW_BOOT_CONTEXT;

#define MPI2_FUNCTION_SCSI_TASK_MGMT                (0x01)

/*
 * MFI command completion codes
 */
enum MFI_STAT {
	MFI_STAT_OK = 0x00,
	MFI_STAT_INVALID_CMD = 0x01,
	MFI_STAT_INVALID_DCMD = 0x02,
	MFI_STAT_INVALID_PARAMETER = 0x03,
	MFI_STAT_INVALID_SEQUENCE_NUMBER = 0x04,
	MFI_STAT_ABORT_NOT_POSSIBLE = 0x05,
	MFI_STAT_APP_HOST_CODE_NOT_FOUND = 0x06,
	MFI_STAT_APP_IN_USE = 0x07,
	MFI_STAT_APP_NOT_INITIALIZED = 0x08,
	MFI_STAT_ARRAY_INDEX_INVALID = 0x09,
	MFI_STAT_ARRAY_ROW_NOT_EMPTY = 0x0a,
	MFI_STAT_CONFIG_RESOURCE_CONFLICT = 0x0b,
	MFI_STAT_DEVICE_NOT_FOUND = 0x0c,
	MFI_STAT_DRIVE_TOO_SMALL = 0x0d,
	MFI_STAT_FLASH_ALLOC_FAIL = 0x0e,
	MFI_STAT_FLASH_BUSY = 0x0f,
	MFI_STAT_FLASH_ERROR = 0x10,
	MFI_STAT_FLASH_IMAGE_BAD = 0x11,
	MFI_STAT_FLASH_IMAGE_INCOMPLETE = 0x12,
	MFI_STAT_FLASH_NOT_OPEN = 0x13,
	MFI_STAT_FLASH_NOT_STARTED = 0x14,
	MFI_STAT_FLUSH_FAILED = 0x15,
	MFI_STAT_HOST_CODE_NOT_FOUNT = 0x16,
	MFI_STAT_LD_CC_IN_PROGRESS = 0x17,
	MFI_STAT_LD_INIT_IN_PROGRESS = 0x18,
	MFI_STAT_LD_LBA_OUT_OF_RANGE = 0x19,
	MFI_STAT_LD_MAX_CONFIGURED = 0x1a,
	MFI_STAT_LD_NOT_OPTIMAL = 0x1b,
	MFI_STAT_LD_RBLD_IN_PROGRESS = 0x1c,
	MFI_STAT_LD_RECON_IN_PROGRESS = 0x1d,
	MFI_STAT_LD_WRONG_RAID_LEVEL = 0x1e,
	MFI_STAT_MAX_SPARES_EXCEEDED = 0x1f,
	MFI_STAT_MEMORY_NOT_AVAILABLE = 0x20,
	MFI_STAT_MFC_HW_ERROR = 0x21,
	MFI_STAT_NO_HW_PRESENT = 0x22,
	MFI_STAT_NOT_FOUND = 0x23,
	MFI_STAT_NOT_IN_ENCL = 0x24,
	MFI_STAT_PD_CLEAR_IN_PROGRESS = 0x25,
	MFI_STAT_PD_TYPE_WRONG = 0x26,
	MFI_STAT_PR_DISABLED = 0x27,
	MFI_STAT_ROW_INDEX_INVALID = 0x28,
	MFI_STAT_SAS_CONFIG_INVALID_ACTION = 0x29,
	MFI_STAT_SAS_CONFIG_INVALID_DATA = 0x2a,
	MFI_STAT_SAS_CONFIG_INVALID_PAGE = 0x2b,
	MFI_STAT_SAS_CONFIG_INVALID_TYPE = 0x2c,
	MFI_STAT_SCSI_DONE_WITH_ERROR = 0x2d,
	MFI_STAT_SCSI_IO_FAILED = 0x2e,
	MFI_STAT_SCSI_RESERVATION_CONFLICT = 0x2f,
	MFI_STAT_SHUTDOWN_FAILED = 0x30,
	MFI_STAT_TIME_NOT_SET = 0x31,
	MFI_STAT_WRONG_STATE = 0x32,
	MFI_STAT_LD_OFFLINE = 0x33,
	MFI_STAT_PEER_NOTIFICATION_REJECTED = 0x34,
	MFI_STAT_PEER_NOTIFICATION_FAILED = 0x35,
	MFI_STAT_RESERVATION_IN_PROGRESS = 0x36,
	MFI_STAT_I2C_ERRORS_DETECTED = 0x37,
	MFI_STAT_PCI_ERRORS_DETECTED = 0x38,
	MFI_STAT_CONFIG_SEQ_MISMATCH = 0x67,

	MFI_STAT_INVALID_STATUS = 0xFF
};

typedef enum {
	MFI_EVT_CLASS_DEBUG =		-2,
	MFI_EVT_CLASS_PROGRESS =	-1,
	MFI_EVT_CLASS_INFO =		0,
	MFI_EVT_CLASS_WARNING =		1,
	MFI_EVT_CLASS_CRITICAL =	2,
	MFI_EVT_CLASS_FATAL =		3,
	MFI_EVT_CLASS_DEAD =		4
} mfi_evt_class_t;

/*
 * Crash dump related defines
 */
#define MAX_CRASH_DUMP_SIZE 512
#define CRASH_DMA_BUF_SIZE  (1024 * 1024)	/* Crash DMA buffer size in bytes*/

typedef enum _MR_FW_CRASH_DUMP_STATE {
        UNAVAILABLE =    0,
        AVAILABLE =      1,
        COPYING =        2,
        COPIED =         3,
        COPY_ERROR =     4,
} MR_FW_CRASH_DUMP_STATE;

typedef enum _MR_CRASH_BUF_STATUS {
    MR_CRASH_BUF_TURN_OFF = 0,
    MR_CRASH_BUF_TURN_ON  = 1
} MR_CRASH_BUF_STATUS;

/* Driver's internal Logging levels*/
#define OCR_LOGS    (1 << 0)

#define SGE_BUFFER_SIZE	4096
/*
 * Number of mailbox bytes in DCMD message frame
 */
#define MFI_MBOX_SIZE				12

enum MR_EVT_CLASS {

	MR_EVT_CLASS_DEBUG = -2,
	MR_EVT_CLASS_PROGRESS = -1,
	MR_EVT_CLASS_INFO = 0,
	MR_EVT_CLASS_WARNING = 1,
	MR_EVT_CLASS_CRITICAL = 2,
	MR_EVT_CLASS_FATAL = 3,
	MR_EVT_CLASS_DEAD = 4,

};

enum MR_EVT_LOCALE {

	MR_EVT_LOCALE_LD = 0x0001,
	MR_EVT_LOCALE_PD = 0x0002,
	MR_EVT_LOCALE_ENCL = 0x0004,
	MR_EVT_LOCALE_BBU = 0x0008,
	MR_EVT_LOCALE_SAS = 0x0010,
	MR_EVT_LOCALE_CTRL = 0x0020,
	MR_EVT_LOCALE_CONFIG = 0x0040,
	MR_EVT_LOCALE_CLUSTER = 0x0080,
	MR_EVT_LOCALE_ALL = 0xffff,

};

enum MR_EVT_ARGS {

	MR_EVT_ARGS_NONE,
	MR_EVT_ARGS_CDB_SENSE,
	MR_EVT_ARGS_LD,
	MR_EVT_ARGS_LD_COUNT,
	MR_EVT_ARGS_LD_LBA,
	MR_EVT_ARGS_LD_OWNER,
	MR_EVT_ARGS_LD_LBA_PD_LBA,
	MR_EVT_ARGS_LD_PROG,
	MR_EVT_ARGS_LD_STATE,
	MR_EVT_ARGS_LD_STRIP,
	MR_EVT_ARGS_PD,
	MR_EVT_ARGS_PD_ERR,
	MR_EVT_ARGS_PD_LBA,
	MR_EVT_ARGS_PD_LBA_LD,
	MR_EVT_ARGS_PD_PROG,
	MR_EVT_ARGS_PD_STATE,
	MR_EVT_ARGS_PCI,
	MR_EVT_ARGS_RATE,
	MR_EVT_ARGS_STR,
	MR_EVT_ARGS_TIME,
	MR_EVT_ARGS_ECC,
	MR_EVT_ARGS_LD_PROP,
	MR_EVT_ARGS_PD_SPARE,
	MR_EVT_ARGS_PD_INDEX,
	MR_EVT_ARGS_DIAG_PASS,
	MR_EVT_ARGS_DIAG_FAIL,
	MR_EVT_ARGS_PD_LBA_LBA,
	MR_EVT_ARGS_PORT_PHY,
	MR_EVT_ARGS_PD_MISSING,
	MR_EVT_ARGS_PD_ADDRESS,
	MR_EVT_ARGS_BITMAP,
	MR_EVT_ARGS_CONNECTOR,
	MR_EVT_ARGS_PD_PD,
	MR_EVT_ARGS_PD_FRU,
	MR_EVT_ARGS_PD_PATHINFO,
	MR_EVT_ARGS_PD_POWER_STATE,
	MR_EVT_ARGS_GENERIC,
};

/*
 * define constants for device list query options
 */
enum MR_PD_QUERY_TYPE {
	MR_PD_QUERY_TYPE_ALL                = 0,
	MR_PD_QUERY_TYPE_STATE              = 1,
	MR_PD_QUERY_TYPE_POWER_STATE        = 2,
	MR_PD_QUERY_TYPE_MEDIA_TYPE         = 3,
	MR_PD_QUERY_TYPE_SPEED              = 4,
	MR_PD_QUERY_TYPE_EXPOSED_TO_HOST    = 5,
};

enum MR_LD_QUERY_TYPE {
	MR_LD_QUERY_TYPE_ALL	         = 0,
	MR_LD_QUERY_TYPE_EXPOSED_TO_HOST = 1,
	MR_LD_QUERY_TYPE_USED_TGT_IDS    = 2,
	MR_LD_QUERY_TYPE_CLUSTER_ACCESS  = 3,
	MR_LD_QUERY_TYPE_CLUSTER_LOCALE  = 4,
};


#define MR_EVT_CFG_CLEARED                              0x0004
#define MR_EVT_LD_STATE_CHANGE                          0x0051
#define MR_EVT_PD_INSERTED                              0x005b
#define MR_EVT_PD_REMOVED                               0x0070
#define MR_EVT_LD_CREATED                               0x008a
#define MR_EVT_LD_DELETED                               0x008b
#define MR_EVT_FOREIGN_CFG_IMPORTED                     0x00db
#define MR_EVT_LD_OFFLINE                               0x00fc
#define MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED             0x0152

enum MR_PD_STATE {
	MR_PD_STATE_UNCONFIGURED_GOOD   = 0x00,
	MR_PD_STATE_UNCONFIGURED_BAD    = 0x01,
	MR_PD_STATE_HOT_SPARE           = 0x02,
	MR_PD_STATE_OFFLINE             = 0x10,
	MR_PD_STATE_FAILED              = 0x11,
	MR_PD_STATE_REBUILD             = 0x14,
	MR_PD_STATE_ONLINE              = 0x18,
	MR_PD_STATE_COPYBACK            = 0x20,
	MR_PD_STATE_SYSTEM              = 0x40,
 };

typedef union	_MR_PD_REF {
	struct {
		u16	 deviceId;
		u16	 seqNum;
	} mrPdRef;
	u32	 ref;
} MR_PD_REF;

/*
 * define the DDF Type bit structure
 */
union MR_PD_DDF_TYPE {
	 struct {
		union {
			struct {
#ifndef __BIG_ENDIAN_BITFIELD
				 u16	 forcedPDGUID:1;
				 u16	 inVD:1;
				 u16	 isGlobalSpare:1;
				 u16	 isSpare:1;
				 u16	 isForeign:1;
				 u16	 reserved:7;
				 u16	 intf:4;
#else
				 u16	 intf:4;
				 u16	 reserved:7;
				 u16	 isForeign:1;
				 u16	 isSpare:1;
				 u16	 isGlobalSpare:1;
				 u16	 inVD:1;
				 u16	 forcedPDGUID:1;
#endif
			 } pdType;
			 u16	 type;
		 };
		 u16	 reserved;
	 } ddf;
	 struct {
		 u32	reserved;
	 } nonDisk;
	 u32	 type;
} __packed;

/*
 * defines the progress structure
 */
union MR_PROGRESS {
    struct  {
        u16     progress;
        union {
            u16     elapsedSecs;
			u16     elapsedSecsForLastPercent;
        };
    } mrProgress;
    u32     w;
} __packed;

/*
 * defines the physical drive progress structure
 */
struct MR_PD_PROGRESS {
    struct {
#ifndef MFI_BIG_ENDIAN
        u32     rbld:1;
        u32     patrol:1;
        u32     clear:1;
        u32     copyBack:1;
        u32     erase:1;
        u32     locate:1;
        u32     reserved:26;
#else
        u32     reserved:26;
        u32     locate:1;
        u32     erase:1;
        u32     copyBack:1;
		u32     clear:1;
		u32     patrol:1;
		u32     rbld:1;
#endif
    } active;
    union MR_PROGRESS     rbld;
    union MR_PROGRESS     patrol;
    union {
        union MR_PROGRESS     clear;
        union MR_PROGRESS     erase;
    };

    struct {
#ifndef MFI_BIG_ENDIAN
        u32     rbld:1;
        u32     patrol:1;
        u32     clear:1;
        u32     copyBack:1;
        u32     erase:1;
        u32     reserved:27;
#else
		u32     reserved:27;
		u32     erase:1;
        u32     copyBack:1;
		u32     clear:1;
		u32     patrol:1;
		u32     rbld:1;
#endif
    } pause;

    union MR_PROGRESS     reserved[3];
} __packed;
 

struct  MR_PD_INFO {
	 MR_PD_REF	 ref;
	 u8		 inquiryData[96];
	 u8		 vpdPage83[64];

	 u8		 notSupported;
	 u8		 scsiDevType;

	 union {
		 u8		 connectedPortBitmap;
		 u8		 connectedPortNumbers;
	 };

	 u8		 deviceSpeed;
	 u32	 mediaErrCount;
	 u32	 otherErrCount;
	 u32	 predFailCount;
	 u32	 lastPredFailEventSeqNum;

	 u16	 fwState;
	 u8		 disabledForRemoval;
	 u8		 linkSpeed;
	 union MR_PD_DDF_TYPE  state;

	 struct {
		 u8		 count;
#ifndef __BIG_ENDIAN_BITFIELD
		 u8		 isPathBroken:4;
		 u8		 reserved3:3;
		 u8		 widePortCapable:1;
#else
		 u8		 widePortCapable:1;
		 u8		 reserved3:3;
		 u8		 isPathBroken:4;
#endif

		 u8		 connectorIndex[2];
		 u8		 reserved[4];
		 u64	 sasAddr[2];
		 u8		 reserved2[16];
	 } pathInfo;

	 u64	 rawSize;
	 u64	 nonCoercedSize;
	 u64	 coercedSize;
	 u16	 enclDeviceId;
	 u8		 enclIndex;

	 union {
		 u8		 slotNumber;
		 u8		 enclConnectorIndex;
	 };

	struct MR_PD_PROGRESS progInfo;
	 u8		 badBlockTableFull;
	 u8		 unusableInCurrentConfig;
	 u8		 vpdPage83Ext[64];
	 u8		 powerState;
	 u8		 enclPosition;
	 u32		allowedOps;
	 u16	 copyBackPartnerId;
	 u16	 enclPartnerDeviceId;
	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		 u16 fdeCapable:1;
		 u16 fdeEnabled:1;
		 u16 secured:1;
		 u16 locked:1;
		 u16 foreign:1;
		 u16 needsEKM:1;
		 u16 reserved:10;
#else
		 u16 reserved:10;
		 u16 needsEKM:1;
		 u16 foreign:1;
		 u16 locked:1;
		 u16 secured:1;
		 u16 fdeEnabled:1;
		 u16 fdeCapable:1;
#endif
	 } security;
	 u8		 mediaType;
	 u8		 notCertified;
	 u8		 bridgeVendor[8];
	 u8		 bridgeProductIdentification[16];
	 u8		 bridgeProductRevisionLevel[4];
	 u8		 satBridgeExists;

	 u8		 interfaceType;
	 u8		 temperature;
	 u8		 emulatedBlockSize;
	 u16	 userDataBlockSize;
	 u16	 reserved2;

	 struct {
#ifndef __BIG_ENDIAN_BITFIELD
		 u32 piType:3;
		 u32 piFormatted:1;
		 u32 piEligible:1;
		 u32 NCQ:1;
		 u32 WCE:1;
		 u32 commissionedSpare:1;
		 u32 emergencySpare:1;
		 u32 ineligibleForSSCD:1;
		 u32 ineligibleForLd:1;
		 u32 useSSEraseType:1;
		 u32 wceUnchanged:1;
		 u32 supportScsiUnmap:1;
		 u32 reserved:18;
 #else
		 u32 reserved:18;
		 u32 supportScsiUnmap:1;
		 u32 wceUnchanged:1;
		 u32 useSSEraseType:1;
		 u32 ineligibleForLd:1;
		 u32 ineligibleForSSCD:1;
		 u32 emergencySpare:1;
		 u32 commissionedSpare:1;
		 u32 WCE:1;
		 u32 NCQ:1;
		 u32 piEligible:1;
		 u32 piFormatted:1;
		 u32 piType:3;
#endif
	 } properties;

	 u64   shieldDiagCompletionTime;
	 u8				 shieldCounter;

	 u8		 linkSpeedOther;
	 u8		 reserved4[2];

	 struct {
#ifndef __BIG_ENDIAN_BITFIELD
		 u32	 bbmErrCountSupported:1;
		 u32	 bbmErrCount:31;
#else
		 u32	 bbmErrCount:31;
		 u32	 bbmErrCountSupported:1;
#endif
	 } bbmErr;

	 u8		 reserved1[512-428];
} __packed;

/*
 * Definition of structure used to expose attributes of VD or JBOD
 * (this structure is to be filled by firmware when MR_DCMD_DRV_GET_TARGET_PROP
 * is fired by driver)
 */
struct MR_TARGET_PROPERTIES {
    u32    max_io_size_kb;
    u32    device_qdepth;
    u32    sector_size;
    u8     reserved[500];
} __packed;

/*
 * defines the physical drive address structure
 */
struct MR_PD_ADDRESS {
	u16     deviceId;
	u16     enclDeviceId;

	union {
		struct {
			u8  enclIndex;
			u8  slotNumber;
		} mrPdAddress;
		struct {
			u8  enclPosition;
			u8  enclConnectorIndex;
		} mrEnclAddress;
	};
	u8      scsiDevType;
	union {
		u8      connectedPortBitmap;
		u8      connectedPortNumbers;
	};
	u64     sasAddr[2];
} __packed;

/*
 * defines the physical drive list structure
 */
struct MR_PD_LIST {
	u32             size;
	u32             count;
	struct MR_PD_ADDRESS   addr[1];
} __packed;

struct megasas_pd_list {
	u16             tid;
	u8             driveType;
	u8             driveState;
} __packed;


 /*
 * defines the logical drive reference structure
 */
typedef union  _MR_LD_REF {        // LD reference structure
    struct {
        u8      targetId;           // LD target id (0 to MAX_TARGET_ID)
        u8      reserved;           // reserved to make in line with MR_PD_REF
        u16     seqNum;             // Sequence Number
    };
    u32     ref;                    // shorthand reference to full 32-bits
} MR_LD_REF;                        // 4 bytes


/*
 * defines the logical drive list structure
 */
struct MR_LD_LIST {
    u32     ldCount;                // number of LDs
    u32     reserved;               // pad to 8-byte boundary
    struct {
        MR_LD_REF   ref;            // LD reference
        u8          state;          // current LD state (MR_LD_STATE)
        u8          reserved[3];    // pad to 8-byte boundary
        u64         size;           // LD size
    } ldList[MAX_LOGICAL_DRIVES_EXT];
} __attribute__ ((packed));

struct MR_LD_TARGETID_LIST {
	u32	size;
	u32	count;
	u8	pad[3];
	u8	targetId[MAX_LOGICAL_DRIVES_EXT];
};

/*
 *  * defines the BIOS data strcuture
 *   */
struct MR_BIOS_DATA {
	u16     bootTargetId;
	u8      doNotExpose;
	u8      continueOnError;
	u8      verbose;
	u8      geometry;
	u8      exposeAllDrives;
	u8      disableCTO;
	u8      bootDeviceIsPD;
	u8      EKMStatus;
	u8      autoSelectBootLd;
	u8      reserved[52];
	u8      checkSum;
};

/*
 * SAS controller properties
 */
struct megasas_ctrl_prop {

	u16 seq_num;
	u16 pred_fail_poll_interval;
	u16 intr_throttle_count;
	u16 intr_throttle_timeouts;
	u8 rebuild_rate;
	u8 patrol_read_rate;
	u8 bgi_rate;
	u8 cc_rate;
	u8 recon_rate;
	u8 cache_flush_interval;
	u8 spinup_drv_count;
	u8 spinup_delay;
	u8 cluster_enable;
	u8 coercion_mode;
	u8 alarm_enable;
	u8 disable_auto_rebuild;
	u8 disable_battery_warn;
	u8 ecc_bucket_size;
	u16 ecc_bucket_leak_rate;
	u8 restore_hotspare_on_insertion;
	u8 expose_encl_devices;
        u8      maintainPdFailHistory;
    u8      disallowHostRequestReordering;
    u8      abortCCOnError;                 // set TRUE to abort CC on detecting an inconsistency
    u8      loadBalanceMode;                // load balance mode (MR_LOAD_BALANCE_MODE)
    u8      disableAutoDetectBackplane;     // 0 - use auto detect logic of backplanes like SGPIO, i2c SEP using h/w mechansim like GPIO pins
                                            // 1 - disable auto detect SGPIO,
                                            // 2 - disable i2c SEP auto detect
                                            // 3 - disable both auto detect
    u8      snapVDSpace;                    // % of source LD to be reserved for a VDs snapshot in snapshot repository, for metadata and user data
                                            // 1=5%, 2=10%, 3=15% and so on

    /*
     * Add properties that can be controlled by a bit in the following structure.
     */
    struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:18;
		u32     enableJBOD:1;
		u32     disableSpinDownHS:1;
		u32     allowBootWithPinnedCache:1;
		u32     disableOnlineCtrlReset:1;
		u32     enableSecretKeyControl:1;
		u32     autoEnhancedImport:1;
		u32     enableSpinDownUnconfigured:1;
		u32     SSDPatrolReadEnabled:1;
		u32     SSDSMARTerEnabled:1;
		u32     disableNCQ:1;
		u32     useFdeOnly:1;
		u32     prCorrectUnconfiguredAreas:1;
		u32     SMARTerEnabled:1;
		u32     copyBackDisabled:1;
#else
		u32     copyBackDisabled:1;
		u32     SMARTerEnabled:1;
		u32     prCorrectUnconfiguredAreas:1;
		u32     useFdeOnly:1;
		u32     disableNCQ:1;
		u32     SSDSMARTerEnabled:1;
		u32     SSDPatrolReadEnabled:1;
		u32     enableSpinDownUnconfigured:1;
		u32     autoEnhancedImport:1;
		u32     enableSecretKeyControl:1;
		u32     disableOnlineCtrlReset:1;
		u32     allowBootWithPinnedCache:1;
		u32     disableSpinDownHS:1;
		u32     enableJBOD:1;
		u32     reserved:18;
#endif
    } OnOffProperties;
    u8      autoSnapVDSpace;                // % of source LD to be reserved for auto snapshot in snapshot repository, for metadata and user data
                                            // 1=5%, 2=10%, 3=15% and so on
    u8      viewSpace;                      // snapshot writeable VIEWs capacity as a % of source LD capacity. 0=READ only
                                            // 1=5%, 2=10%, 3=15% and so on

    u16     spinDownTime;                   // # of idle minutes before device is spun down (0=use FW defaults)

    u8      reserved[24];

} __packed;

/*
 * SAS controller information
 */
struct megasas_ctrl_info {

	/*
	 * PCI device information
	 */
	struct {

		u16 vendor_id;
		u16 device_id;
		u16 sub_vendor_id;
		u16 sub_device_id;
		u8 reserved[24];

	} __attribute__ ((packed)) pci;

	/*
	 * Host interface information
	 */
	struct {

		u8 PCIX:1;
		u8 PCIE:1;
		u8 iSCSI:1;
		u8 SAS_3G:1;
		u8 SRIOV:1;
		u8 reserved_0:3;
		u8 reserved_1[6];
		u8 port_count;
		u64 port_addr[8];

	} __attribute__ ((packed)) host_interface;

	/*
	 * Device (backend) interface information
	 */
	struct {

		u8 SPI:1;
		u8 SAS_3G:1;
		u8 SATA_1_5G:1;
		u8 SATA_3G:1;
		u8 reserved_0:4;
		u8 reserved_1[6];
		u8 port_count;
		u64 port_addr[8];

	} __attribute__ ((packed)) device_interface;

	/*
	 * List of components residing in flash. All str are null terminated
	 */
	u32 image_check_word;
	u32 image_component_count;

	struct {

		char name[8];
		char version[32];
		char build_date[16];
		char built_time[16];

	} __attribute__ ((packed)) image_component[8];

	/*
	 * List of flash components that have been flashed on the card, but
	 * are not in use, pending reset of the adapter. This list will be
	 * empty if a flash operation has not occurred. All stings are null
	 * terminated
	 */
	u32 pending_image_component_count;

	struct {

		char name[8];
		char version[32];
		char build_date[16];
		char build_time[16];

	} __attribute__ ((packed)) pending_image_component[8];

	u8 max_arms;
	u8 max_spans;
	u8 max_arrays;
	u8 max_lds;

	char product_name[80];
	char serial_no[32];

	/*
	 * Other physical/controller/operation information. Indicates the
	 * presence of the hardware
	 */
	struct {

		u32 bbu:1;
		u32 alarm:1;
		u32 nvram:1;
		u32 uart:1;
		u32 reserved:28;

	} __attribute__ ((packed)) hw_present;

	u32 current_fw_time;

	/*
	 * Maximum data transfer sizes
	 */
	u16 max_concurrent_cmds;
	u16 max_sge_count;
	u32 max_request_size;

	/*
	 * Logical and physical device counts
	 */
	u16 ld_present_count;
	u16 ld_degraded_count;
	u16 ld_offline_count;

	u16 pd_present_count;
	u16 pd_disk_present_count;
	u16 pd_disk_pred_failure_count;
	u16 pd_disk_failed_count;

	/*
	 * Memory size information
	 */
	u16 nvram_size;
	u16 memory_size;
	u16 flash_size;

	/*
	 * Error counters
	 */
	u16 mem_correctable_error_count;
	u16 mem_uncorrectable_error_count;

	/*
	 * Cluster information
	 */
	u8 cluster_permitted;
	u8 cluster_active;

	/*
	 * Additional max data transfer sizes
	 */
	u16 max_strips_per_io;

	/*
	 * Controller capabilities structures
	 */
	struct {

		u32 raid_level_0:1;
		u32 raid_level_1:1;
		u32 raid_level_5:1;
		u32 raid_level_1E:1;
		u32 raid_level_6:1;
		u32 reserved:27;

	} __attribute__ ((packed)) raid_levels;

	struct {

		u32 rbld_rate:1;
		u32 cc_rate:1;
		u32 bgi_rate:1;
		u32 recon_rate:1;
		u32 patrol_rate:1;
		u32 alarm_control:1;
		u32 cluster_supported:1;
		u32 bbu:1;
		u32 spanning_allowed:1;
		u32 dedicated_hotspares:1;
		u32 revertible_hotspares:1;
		u32 foreign_config_import:1;
		u32 self_diagnostic:1;
		u32 mixed_redundancy_arr:1;
		u32 global_hot_spares:1;
		u32 reserved:17;

	} __attribute__ ((packed)) adapter_operations;

	struct {

#if   defined(__BIG_ENDIAN_BITFIELD)
		u32 reserved:27;
		u32 disk_cache_policy:1;
		u32 access_policy:1;
		u32 io_policy:1;
		u32 write_policy:1;
		u32 read_policy:1;
#else
		u32 read_policy:1;
		u32 write_policy:1;
		u32 io_policy:1;
		u32 access_policy:1;
		u32 disk_cache_policy:1;
		u32 reserved:27;
#endif

	} __attribute__ ((packed)) ld_operations;

	struct {

		u8 min;
		u8 max;
		u8 reserved[2];

	} __attribute__ ((packed)) stripe_sz_ops;

	struct {

		u32 force_online:1;
		u32 force_offline:1;
		u32 force_rebuild:1;
		u32 reserved:29;

	} __attribute__ ((packed)) pd_operations;

	struct {

		u32 ctrl_supports_sas:1;
		u32 ctrl_supports_sata:1;
		u32 allow_mix_in_encl:1;
		u32 allow_mix_in_ld:1;
		u32 allow_sata_in_cluster:1;
		u32 reserved:27;

	} __attribute__ ((packed)) pd_mix_support;

	/*
	 * Define ECC single-bit-error bucket information
	 */
	u8 ecc_bucket_count;
	u8 reserved_2[11];

	/*
	 * Include the controller properties (changeable items)
	 */
	struct megasas_ctrl_prop properties;

	/*
	 * Define FW pkg version (set in envt v'bles on OEM basis)
	 */
	char package_version[0x60];

    
	/*
	* If adapterOperations.supportMoreThan8Phys is set, and deviceInterface.portCount is greater than 8,
	* SAS Addrs for first 8 ports shall be populated in deviceInterface.portAddr, and the rest shall be
	* populated in deviceInterfacePortAddr2.
	*/
	u64         deviceInterfacePortAddr2[8]; //0x6a0 
	u8          reserved3[128];              //0x6e0 
						     
	struct {                                //0x760
		u16 minPdRaidLevel_0                : 4;
		u16 maxPdRaidLevel_0                : 12;

		u16 minPdRaidLevel_1                : 4;
		u16 maxPdRaidLevel_1                : 12;

		u16 minPdRaidLevel_5                : 4;
		u16 maxPdRaidLevel_5                : 12;

		u16 minPdRaidLevel_1E               : 4;
		u16 maxPdRaidLevel_1E               : 12;

		u16 minPdRaidLevel_6                : 4;
		u16 maxPdRaidLevel_6                : 12;

		u16 minPdRaidLevel_10               : 4;
		u16 maxPdRaidLevel_10               : 12;

		u16 minPdRaidLevel_50               : 4;
		u16 maxPdRaidLevel_50               : 12;

		u16 minPdRaidLevel_60               : 4;
		u16 maxPdRaidLevel_60               : 12;

		u16 minPdRaidLevel_1E_RLQ0          : 4;
		u16 maxPdRaidLevel_1E_RLQ0          : 12;

		u16 minPdRaidLevel_1E0_RLQ0         : 4;
		u16 maxPdRaidLevel_1E0_RLQ0         : 12;

		u16 reserved[6];                    
	} pdsForRaidLevels;

	u16 maxPds;                             //0x780 
	u16 maxDedHSPs;                         //0x782 
	u16 maxGlobalHSPs;                      //0x784 
	u16 ddfSize;                            //0x786 
	u8  maxLdsPerArray;                     //0x788 
	u8  partitionsInDDF;                    //0x789 
	u8  lockKeyBinding;                     //0x78a 
	u8  maxPITsPerLd;                       //0x78b 
	u8  maxViewsPerLd;                      //0x78c 
	u8  maxTargetId;                        //0x78d 
	u16 maxBvlVdSize;                       //0x78e 

	u16 maxConfigurableSSCSize;             //0x790 
	u16 currentSSCsize;                     //0x792 

	char    expanderFwVersion[12];          //0x794 

	u16 PFKTrialTimeRemaining;              //0x7A0 

	u16 cacheMemorySize;                    //0x7A2 

	struct {                                //0x7A4
#if   defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:5;
		u32	activePassive:2;
		u32	supportConfigAutoBalance:1;
		u32	mpio:1;
		u32	supportDataLDonSSCArray:1;
		u32	supportPointInTimeProgress:1;
		u32     supportUnevenSpans:1;
		u32     dedicatedHotSparesLimited:1;
		u32     headlessMode:1;
		u32     supportEmulatedDrives:1;
		u32     supportResetNow:1;
		u32     realTimeScheduler:1;
		u32     supportSSDPatrolRead:1;
		u32     supportPerfTuning:1;
		u32     disableOnlinePFKChange:1;
		u32     supportJBOD:1;
		u32     supportBootTimePFKChange:1;
		u32     supportSetLinkSpeed:1;
		u32     supportEmergencySpares:1;
		u32     supportSuspendResumeBGops:1;
		u32     blockSSDWriteCacheChange:1;
		u32     supportShieldState:1;
		u32     supportLdBBMInfo:1;
		u32     supportLdPIType3:1;
		u32     supportLdPIType2:1;
		u32     supportLdPIType1:1;
		u32     supportPIcontroller:1;
#else
		u32     supportPIcontroller         :1;         
		u32     supportLdPIType1            :1;         
		u32     supportLdPIType2            :1;         
		u32     supportLdPIType3            :1;         
		u32     supportLdBBMInfo            :1;         
		u32     supportShieldState          :1;         
		u32     blockSSDWriteCacheChange    :1;         
		u32     supportSuspendResumeBGops   :1;         
		u32     supportEmergencySpares      :1;         
		u32     supportSetLinkSpeed         :1;         
		u32     supportBootTimePFKChange    :1;         
		u32     supportJBOD                 :1;         
		u32     disableOnlinePFKChange      :1;         
		u32     supportPerfTuning           :1;         
		u32     supportSSDPatrolRead        :1;         
		u32     realTimeScheduler           :1;         
								
		u32     supportResetNow             :1;         
		u32     supportEmulatedDrives       :1;         
		u32     headlessMode                :1;         
		u32     dedicatedHotSparesLimited   :1;         
								
								
		u32     supportUnevenSpans          :1;
		u32	supportPointInTimeProgress  :1;
		u32	supportDataLDonSSCArray     :1;
		u32	mpio                        :1;
		u32	supportConfigAutoBalance    :1;
		u32	activePassive               :2;
		u32     reserved                    :5;
#endif       
	} adapterOperations2;

	u8  driverVersion[32];                  //0x7A8 
	u8  maxDAPdCountSpinup60;               //0x7C8 
	u8  temperatureROC;                     //0x7C9 
	u8  temperatureCtrl;                    //0x7CA 
	u8  reserved4;                          //0x7CB
	u16 maxConfigurablePds;                 //0x7CC 
						    

	u8  reserved5[2];                       //0x7CD reserved for future use

	/*
	* HA cluster information
	*/
	struct {
	#if defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved                    :25;
		u32     passive			    :1;
		u32     premiumFeatureMismatch      :1;         
		u32     ctrlPropIncompatible        :1;         
		u32     fwVersionMismatch           :1;         
		u32     hwIncompatible              :1;         
		u32     peerIsIncompatible          :1;         
		u32     peerIsPresent               :1;         
	#else	
		u32     peerIsPresent               :1;         
		u32     peerIsIncompatible          :1;         
								
		u32     hwIncompatible              :1;         
		u32     fwVersionMismatch           :1;         
		u32     ctrlPropIncompatible        :1;         
		u32     premiumFeatureMismatch      :1;
		u32     passive			    :1;
		u32     reserved                    :25;
	#endif
	} cluster;

	char clusterId[MEGASAS_CLUSTER_ID_SIZE];                     //0x7D4 
	struct {
		u8  maxVFsSupported;            /*0x7E4*/
		u8  numVFsEnabled;              /*0x7E5*/
		u8  requestorId;                /*0x7E6 0:PF, 1:VF1, 2:VF2*/
		u8  reserved;                   /*0x7E7*/
	} iov;

        struct {                                //0x7E8
	#if defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:7;
		u32     useSeqNumJbodFP:1;
		u32     supportExtendedSSCSize:1;
		u32     supportDiskCacheSettingForSysPDs:1;
		u32     supportCPLDUpdate:1;
		u32     supportTTYLogCompression:1;
		u32     discardCacheDuringLDDelete:1;
		u32     supportSecurityonJBOD:1;
		u32     supportCacheBypassModes:1;
		u32     supportDisableSESMonitoring:1;
		u32     supportForceFlash:1;
		u32     supportNVDRAM:1;
		u32     supportDrvActivityLEDSetting:1;
		u32     supportAllowedOpsforDrvRemoval:1;
		u32     supportHOQRebuild:1;
		u32     supportForceTo512e:1;
		u32     supportNVCacheErase:1;
		u32     supportDebugQueue:1;
		u32     supportSwZone:1;
		u32     supportCrashDump:1;
		u32     supportMaxExtLDs:1;
		u32     supportT10RebuildAssist:1;
		u32     supportDisableImmediateIO:1;
		u32     supportThermalPollInterval:1;
		u32     supportPersonalityChange:2;
	#else
		u32     supportPersonalityChange:2;
		u32     supportThermalPollInterval:1;
		u32     supportDisableImmediateIO:1;
		u32     supportT10RebuildAssist:1;
		u32     supportMaxExtLDs:1;
		u32     supportCrashDump:1;
		u32     supportSwZone:1;
		u32     supportDebugQueue:1;
		u32     supportNVCacheErase:1;
		u32     supportForceTo512e:1;
		u32     supportHOQRebuild:1;
		u32     supportAllowedOpsforDrvRemoval:1;
		u32     supportDrvActivityLEDSetting:1;
		u32     supportNVDRAM:1;
		u32     supportForceFlash:1;
		u32     supportDisableSESMonitoring:1;
		u32     supportCacheBypassModes:1;
		u32     supportSecurityonJBOD:1;
		u32     discardCacheDuringLDDelete:1;
		u32     supportTTYLogCompression:1;
		u32     supportCPLDUpdate:1;
		u32     supportDiskCacheSettingForSysPDs:1;
		u32     supportExtendedSSCSize:1;
		u32     useSeqNumJbodFP:1;
		u32     reserved:7;
	#endif
            } adapterOperations3;

		struct {
	#if defined(__BIG_ENDIAN_BITFIELD)
	 	u8 reserved:7;
        u8 cpldInFlash:1;                                // Indicates whether the CPLD image is part of the package and stored in flash        
	#else
		u8 cpldInFlash:1;                                // Indicates whether the CPLD image is part of the package and stored in flash
        u8 reserved:7;       
	#endif
        u8 reserved1 [3];
        u8 userCodeDefinition [12];                      // Null terminated string. Has the version information if cpldInFlash = FALSE
    } cpld;                                              // Valid only if upgradableCPLD is TRUE

	
	struct {
	#if defined(__BIG_ENDIAN_BITFIELD)
		u16 reserved            :2;
		u16 supportNVMePassthru :1;
		u16 supportPLDebugInfo  :1;
		u16 supportFlashCompInfo:1;
		u16 supportHostInfo     :1;
		u16 supportDualFWUpdate :1;
		u16 supportSSCrev3      :1;
		u16 FWSwapsBBUVPDInfo	:1;
		u16 supportPdMapTargetId:1;
		u16 supportSESCtrlInMultipathCfg:1;
		u16 imageUploadSupported:1;
		u16 supportEncryptedMfc :1;
		u16 supportedEncAlgo	:1;
		u16 supportIbuttonLess	:1;
		u16 ctrlInfoExtSupported:1;
	#else

		u16 ctrlInfoExtSupported:1;
		u16 supportIbuttonLess	:1;
		u16 supportedEncAlgo	:1;
		u16 supportEncryptedMfc :1;
		u16 imageUploadSupported:1;
		u16 supportSESCtrlInMultipathCfg:1; 			 // FW supports LUN based association and target port based
														 // association for the SES device connected in multipath mode
		u16 supportPdMapTargetId:1; 					 // FW defines Jbod target Id within MR_PD_CFG_SEQ
		u16 FWSwapsBBUVPDInfo	:1;   // FW swaps relevant fields in MR_BBU_VPD_INFO_FIXED to provide the data in little endian order
		u16 supportSSCrev3      :1;                      // FW supports CacheCade 3.0, only one SSCD creation allowed
		u16 supportDualFWUpdate :1;                      // FW supports dual firmware update feature
		u16 supportHostInfo     :1;                      // FW supports MR_DCMD_CTRL_HOST_INFO_SET/GET
		u16 supportFlashCompInfo:1;                      // FW supports MR_DCMD_CTRL_FLASH_COMP_INFO_GET
		u16 supportPLDebugInfo  :1;                      // FW supports retrieval of PL debug information through apps
		u16 supportNVMePassthru :1;                      // FW supports NVMe passthru commands
		u16 reserved            :2;
	#endif	
		}adapterOperations4;

    u8          pad[0x800-0x7FE]; 		/* 0x7FE pad to 2K for expansion */	
} __packed;



/*
 * ===============================
 * MegaRAID SAS driver definitions
 * ===============================
 */
#define MEGASAS_MAX_NAME                        32
#define MEGASAS_MAX_SECTORS                    (2*1024)
#define MEGASAS_MAX_SECTORS_IEEE               (2*128)
#define MEGASAS_DBG_LVL				1

#define MEGASAS_FW_BUSY				1

/* Frame Type */
#define IO_FRAME				0
#define PTHRU_FRAME				1

/*
 * When SCSI mid-layer calls driver's reset routine, driver waits for
 * MEGASAS_RESET_WAIT_TIME seconds for all outstanding IO to complete. Note
 * that the driver cannot _actually_ abort or reset pending commands. While
 * it is waiting for the commands to complete, it prints a diagnostic message
 * every MEGASAS_RESET_NOTICE_INTERVAL seconds
 */
#define MEGASAS_RESET_WAIT_TIME			180
#define MEGASAS_INTERNAL_CMD_WAIT_TIME		180
#define	MEGASAS_RESET_NOTICE_INTERVAL		5
#define MEGASAS_IOCTL_CMD			0
#define MEGASAS_DEFAULT_CMD_TIMEOUT		90
#define MEGASAS_THROTTLE_QUEUE_DEPTH		16

/*
 * FW reports the maximum of number of commands that it can accept (maximum
 * commands that can be outstanding) at any time. The driver must report a
 * lower number to the mid layer because it can issue a few internal commands
 * itself (E.g, AEN, abort cmd, IOCTLs etc). The number of commands it needs
 * is shown below
 */
#define MEGASAS_INT_CMDS			32
#define MEGASAS_SKINNY_INT_CMDS			5
#define MEGASAS_FUSION_INTERNAL_CMDS		8
#define MEGASAS_FUSION_IOCTL_CMDS		3

#define MEGASAS_MAX_MSIX_QUEUES			128
/*
 * FW can accept both 32 and 64 bit SGLs. We want to allocate 32/64 bit
 * SGLs based on the size of dma_addr_t
 */
#define IS_DMA64				(sizeof(dma_addr_t) == 8)

#define MFI_XSCALE_OMR0_CHANGE_INTERRUPT            0x00000001  /* MFI state change interrupt */

#define MFI_INTR_FLAG_REPLY_MESSAGE                 0x00000001
#define MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE         0x00000002
#define MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT 0x00000004  /* MFI state change interrrupt */

#define MFI_OB_INTR_STATUS_MASK			0x00000002
#define MFI_POLL_TIMEOUT_SECS			60
#define MFI_IO_TIMEOUT_SECS			180
#define MEGASAS_SRIOV_HEARTBEAT_INTERVAL_VF	(5 * HZ)
#define MEGASAS_OCR_SETTLE_TIME_VF		(1000 * 30)
#define MEGASAS_ROUTINE_WAIT_TIME_VF		300
#define MFI_REPLY_1078_MESSAGE_INTERRUPT	0x80000000
#define MFI_REPLY_GEN2_MESSAGE_INTERRUPT	0x00000001
#define MFI_GEN2_ENABLE_INTERRUPT_MASK		(0x00000001 | 0x00000004) 
#define MFI_REPLY_SKINNY_MESSAGE_INTERRUPT	0x40000000
#define MFI_SKINNY_ENABLE_INTERRUPT_MASK	(0x00000001)
#define MFI_1068_PCSR_OFFSET			0x84
#define MFI_1068_FW_HANDSHAKE_OFFSET		0x64
#define MFI_1068_FW_READY			0xDDDD0000

/*       U32     maxReplyQueues   : 5;
 *       U32     maxChainSize     : 5;
 *       U32     maxChainBuffers  : 4;
 *       U32     maxReplyQueuesExt: 8;
 *       U32     maxChainSizeUnit : 1;
 *       U32     RDPQArrayMode    : 1;
 *       U32     canHandleSyncCache : 1;
 *       U32     reserved1        : 3;
 *       U32     state            : 4;
 */

#define MR_MAX_REPLY_QUEUES_OFFSET		0X0000001F
#define MR_MAX_REPLY_QUEUES_EXT_OFFSET		0X003FC000
#define MR_MAX_REPLY_QUEUES_EXT_OFFSET_SHIFT	14
#define MR_MAX_MSIX_REG_ARRAY			16

#define MR_RDPQ_MODE_OFFSET			0X00800000
#define MR_CAN_HANDLE_SYNC_CACHE_OFFSET     0X01000000
#define MR_CAN_HANDLE_64_BIT_DMA_OFFSET     (1 << 25)

#define MR_MAX_RAID_MAP_SIZE_OFFSET_SHIFT	16
#define MR_MAX_RAID_MAP_SIZE_MASK		0x1FF
#define MR_MIN_MAP_SIZE				0x10000 //64k

#define MR_NVME_PAGE_SIZE_MASK		0x000000FF

#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 7) && (RHEL_MINOR >= 3)) || \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)))
#define KERNEL_SUPPORT_BLK_QUEUE_VIRT_BOUNDARY_API

#endif

enum MR_ADAPTER_TYPE {
	MFI_SERIES = 1,
	THUNDERBOLT_SERIES = 2,
	INVADER_SERIES = 3,
	VENTURA_SERIES = 4,
};

/*
* register set for both 1068 and 1078 controllers
* structure extended for 1078 registers
*/
 
struct megasas_register_set {
	u32	doorbell;                       /*0000h*/
	u32	fusion_seq_offset;		/*0004h*/
	u32	fusion_host_diag;		/*0008h*/
	u32 	reserved_01;			/*000Ch*/

	u32 	inbound_msg_0;			/*0010h*/
	u32 	inbound_msg_1;			/*0014h*/
	u32 	outbound_msg_0;			/*0018h*/
	u32 	outbound_msg_1;			/*001Ch*/

	u32 	inbound_doorbell;		/*0020h*/
	u32 	inbound_intr_status;		/*0024h*/
	u32 	inbound_intr_mask;		/*0028h*/

	u32 	outbound_doorbell;		/*002Ch*/
	u32 	outbound_intr_status;		/*0030h*/
	u32 	outbound_intr_mask;		/*0034h*/

	u32 	reserved_1[2];			/*0038h*/

	u32 	inbound_queue_port;		/*0040h*/
	u32 	outbound_queue_port;		/*0044h*/

	u32 	reserved_2[9];			/*0048h*/
	u32 	reply_post_host_index;		/*006Ch*/
	u32 	reserved_2_2[12];		/*0070h*/

	u32 	outbound_doorbell_clear;	/*00A0h*/

	u32 	reserved_3[3];			/*00A4h*/

	u32 	outbound_scratch_pad ;		/*00B0h*/
	u32		outbound_scratch_pad_2;         /*00B4h*/
	u32		outbound_scratch_pad_3;         /*00B8h*/
	u32		outbound_scratch_pad_4;         /*00BCh*/

	u32 	inbound_low_queue_port ;	/*00C0h*/

	u32 	inbound_high_queue_port ;	/*00C4h*/

	u32 	inbound_single_queue_port;			/*00C8h*/
	u32		res_6[11];			/*CCh*/
	u32		host_diag;
	u32		seq_offset;
	u32 	index_registers[807];		/*00CCh*/

} __attribute__ ((packed));

struct megasas_sge32 {

	u32 phys_addr;
	u32 length;

} __attribute__ ((packed));

struct megasas_sge64 {

	u64 phys_addr;
	u32 length;

} __attribute__ ((packed));

struct megasas_sge_skinny {
	u64 phys_addr;
	u32 length;
	u32 flag;
} __packed;

union megasas_sgl {

	struct megasas_sge32 sge32[1];
	struct megasas_sge64 sge64[1];
	struct megasas_sge_skinny sge_skinny[1];

} __attribute__ ((packed));

struct megasas_header {

	u8 cmd;			/*00h */
	u8 sense_len;		/*01h */
	u8 cmd_status;		/*02h */
	u8 scsi_status;		/*03h */

	u8 target_id;		/*04h */
	u8 lun;			/*05h */
	u8 cdb_len;		/*06h */
	u8 sge_count;		/*07h */

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 timeout;		/*12h */
	u32 data_xferlen;	/*14h */

} __attribute__ ((packed));

union megasas_sgl_frame {

	struct megasas_sge32 sge32[8];
	struct megasas_sge64 sge64[5];

} __attribute__ ((packed));

typedef union _MFI_CAPABILITIES {
	struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:17;
		u32		support_nvme_passthru:1;
		u32     support_64bit_mode:1;
        u32		supportPdMapTargetId:1;
		u32     support_qd_throttling:1;
		u32     support_fp_rlbypass:1;
		u32     support_vfid_in_ioframe:1;
		u32     support_ext_io_size:1;
		u32	support_ext_queue_depth:1;
		u32     security_protocol_cmds_fw:1;
        	u32     support_core_affinity:1;
		u32     support_ndrive_r1_lb:1;
		u32	support_max_255lds:1;
		u32	support_fastpath_wb:1;
		u32     support_additional_msix:1;
		u32     support_fp_remote_lun:1;
#else
		u32     support_fp_remote_lun:1;
		u32     support_additional_msix:1;
		u32	support_fastpath_wb:1;
		u32	support_max_255lds:1;
		u32     support_ndrive_r1_lb:1;
        	u32     support_core_affinity:1;
		u32     security_protocol_cmds_fw:1;
		u32	support_ext_queue_depth:1;
		u32     support_ext_io_size:1;
		u32     support_vfid_in_ioframe:1;
		u32     support_fp_rlbypass:1;
		u32     support_qd_throttling:1;
		u32		supportPdMapTargetId:1;
		u32     support_64bit_mode:1;
		u32		support_nvme_passthru:1;
		u32     reserved:17;
#endif
	} mfi_capabilities;
	u32     reg;
} MFI_CAPABILITIES;

struct megasas_init_frame {

	u8 cmd;			/*00h */
	u8 reserved_0;		/*01h */
	u8 cmd_status;		/*02h */

	u8 reserved_1;		/*03h */
	MFI_CAPABILITIES driver_operations; /*04h*/

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 reserved_3;		/*12h */
	u32 data_xfer_len;	/*14h */

	u32 queue_info_new_phys_addr_lo;	/*18h */
	u32 queue_info_new_phys_addr_hi;	/*1Ch */
	u32 queue_info_old_phys_addr_lo;	/*20h */
	u32 queue_info_old_phys_addr_hi;	/*24h */
	u32 driver_ver_lo;      /*28h */
	u32 driver_ver_hi;      /*2Ch */
	u32 system_info_lo;      /*30h */
	u32 system_info_hi;      /*34h */
	u32 reserved_4[2];	/*38h */

} __attribute__ ((packed));

struct megasas_init_queue_info {

	u32 init_flags;		/*00h */
	u32 reply_queue_entries;	/*04h */

	u32 reply_queue_start_phys_addr_lo;	/*08h */
	u32 reply_queue_start_phys_addr_hi;	/*0Ch */
	u32 producer_index_phys_addr_lo;	/*10h */
	u32 producer_index_phys_addr_hi;	/*14h */
	u32 consumer_index_phys_addr_lo;	/*18h */
	u32 consumer_index_phys_addr_hi;	/*1Ch */

} __attribute__ ((packed));

struct megasas_io_frame {

	u8 cmd;			/*00h */
	u8 sense_len;		/*01h */
	u8 cmd_status;		/*02h */
	u8 scsi_status;		/*03h */

	u8 target_id;		/*04h */
	u8 access_byte;		/*05h */
	u8 reserved_0;		/*06h */
	u8 sge_count;		/*07h */

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 timeout;		/*12h */
	u32 lba_count;		/*14h */

	u32 sense_buf_phys_addr_lo;	/*18h */
	u32 sense_buf_phys_addr_hi;	/*1Ch */

	u32 start_lba_lo;	/*20h */
	u32 start_lba_hi;	/*24h */

	union megasas_sgl sgl;	/*28h */

} __attribute__ ((packed));

struct megasas_pthru_frame {

	u8 cmd;			/*00h */
	u8 sense_len;		/*01h */
	u8 cmd_status;		/*02h */
	u8 scsi_status;		/*03h */

	u8 target_id;		/*04h */
	u8 lun;			/*05h */
	u8 cdb_len;		/*06h */
	u8 sge_count;		/*07h */

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 timeout;		/*12h */
	u32 data_xfer_len;	/*14h */

	u32 sense_buf_phys_addr_lo;	/*18h */
	u32 sense_buf_phys_addr_hi;	/*1Ch */

	u8 cdb[16];		/*20h */
	union megasas_sgl sgl;	/*30h */

} __attribute__ ((packed));

struct megasas_dcmd_frame {

	u8 cmd;			/*00h */
	u8 reserved_0;		/*01h */
	u8 cmd_status;		/*02h */
	u8 reserved_1[4];	/*03h */
	u8 sge_count;		/*07h */

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 timeout;		/*12h */

	u32 data_xfer_len;	/*14h */
	u32 opcode;		/*18h */

	union {			/*1Ch */
		u8 b[12];
		u16 s[6];
		u32 w[3];
	} mbox;

	union megasas_sgl sgl;	/*28h */

} __attribute__ ((packed));

struct megasas_abort_frame {

	u8 cmd;			/*00h */
	u8 reserved_0;		/*01h */
	u8 cmd_status;		/*02h */

	u8 reserved_1;		/*03h */
	u32 reserved_2;		/*04h */

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 reserved_3;		/*12h */
	u32 reserved_4;		/*14h */

	u32 abort_context;	/*18h */
	u32 pad_1;		/*1Ch */

	u32 abort_mfi_phys_addr_lo;	/*20h */
	u32 abort_mfi_phys_addr_hi;	/*24h */

	u32 reserved_5[6];	/*28h */

} __attribute__ ((packed));

struct megasas_smp_frame {

	u8 cmd;			/*00h */
	u8 reserved_1;		/*01h */
	u8 cmd_status;		/*02h */
	u8 connection_status;	/*03h */

	u8 reserved_2[3];	/*04h */
	u8 sge_count;		/*07h */

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 timeout;		/*12h */

	u32 data_xfer_len;	/*14h */
	u64 sas_addr;		/*18h */

	union {
		struct megasas_sge32 sge32[2];	/* [0]: resp [1]: req */
		struct megasas_sge64 sge64[2];	/* [0]: resp [1]: req */
	} sgl;

} __attribute__ ((packed));

struct megasas_stp_frame {

	u8 cmd;			/*00h */
	u8 reserved_1;		/*01h */
	u8 cmd_status;		/*02h */
	u8 reserved_2;		/*03h */

	u8 target_id;		/*04h */
	u8 reserved_3[2];	/*05h */
	u8 sge_count;		/*07h */

	u32 context;		/*08h */
	u32 pad_0;		/*0Ch */

	u16 flags;		/*10h */
	u16 timeout;		/*12h */

	u32 data_xfer_len;	/*14h */

	u16 fis[10];		/*18h */
	u32 stp_flags;

	union {
		struct megasas_sge32 sge32[2];	/* [0]: resp [1]: data */
		struct megasas_sge64 sge64[2];	/* [0]: resp [1]: data */
	} sgl;

} __attribute__ ((packed));

union megasas_frame {

	struct megasas_header hdr;
	struct megasas_init_frame init;
	struct megasas_io_frame io;
	struct megasas_pthru_frame pthru;
	struct megasas_dcmd_frame dcmd;
	struct megasas_abort_frame abort;
	struct megasas_smp_frame smp;
	struct megasas_stp_frame stp;
	u8 raw_bytes[64];
};

/**
 * struct MR_PRIV_DEVICE - sdev private hostdata
 * @is_tm_capable: firmware managed tm capable flag
 * @tm_busy: TM request is in progress
 */
struct MR_PRIV_DEVICE {
	bool is_tm_capable;
	bool tm_busy;
	atomic_t r1_ldio_hint;
	u8	interface_type;
};

struct megasas_cmd;

union megasas_evt_class_locale {

	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u16 locale;
		u8 reserved;
		s8 class;
#else
		s8 class;
		u8 reserved;
		u16 locale;
#endif
	} __attribute__ ((packed)) members;

	u32 word;

} __attribute__ ((packed));

struct megasas_evt_log_info {
	u32 newest_seq_num;
	u32 oldest_seq_num;
	u32 clear_seq_num;
	u32 shutdown_seq_num;
	u32 boot_seq_num;

} __attribute__ ((packed));

struct megasas_progress {

	u16 progress;
	u16 elapsed_seconds;

} __attribute__ ((packed));

struct megasas_evtarg_ld {

	u16 target_id;
	u8 ld_index;
	u8 reserved;

} __attribute__ ((packed));

struct megasas_evtarg_pd {
	u16 device_id;
	u8 encl_index;
	u8 slot_number;

} __attribute__ ((packed));

struct megasas_evt_detail {

	u32 seq_num;
	u32 time_stamp;
	u32 code;
	union megasas_evt_class_locale cl;
	u8 arg_type;
	u8 reserved1[15];

	union {
		struct {
			struct megasas_evtarg_pd pd;
			u8 cdb_length;
			u8 sense_length;
			u8 reserved[2];
			u8 cdb[16];
			u8 sense[64];
		} __attribute__ ((packed)) cdbSense;

		struct megasas_evtarg_ld ld;

		struct {
			struct megasas_evtarg_ld ld;
			u64 count;
		} __attribute__ ((packed)) ld_count;

		struct {
			u64 lba;
			struct megasas_evtarg_ld ld;
		} __attribute__ ((packed)) ld_lba;

		struct {
			struct megasas_evtarg_ld ld;
			u32 prevOwner;
			u32 newOwner;
		} __attribute__ ((packed)) ld_owner;

		struct {
			u64 ld_lba;
			u64 pd_lba;
			struct megasas_evtarg_ld ld;
			struct megasas_evtarg_pd pd;
		} __attribute__ ((packed)) ld_lba_pd_lba;

		struct {
			struct megasas_evtarg_ld ld;
			struct megasas_progress prog;
		} __attribute__ ((packed)) ld_prog;

		struct {
			struct megasas_evtarg_ld ld;
			u32 prev_state;
			u32 new_state;
		} __attribute__ ((packed)) ld_state;

		struct {
			u64 strip;
			struct megasas_evtarg_ld ld;
		} __attribute__ ((packed)) ld_strip;

		struct megasas_evtarg_pd pd;

		struct {
			struct megasas_evtarg_pd pd;
			u32 err;
		} __attribute__ ((packed)) pd_err;

		struct {
			u64 lba;
			struct megasas_evtarg_pd pd;
		} __attribute__ ((packed)) pd_lba;

		struct {
			u64 lba;
			struct megasas_evtarg_pd pd;
			struct megasas_evtarg_ld ld;
		} __attribute__ ((packed)) pd_lba_ld;

		struct {
			struct megasas_evtarg_pd pd;
			struct megasas_progress prog;
		} __attribute__ ((packed)) pd_prog;

		struct {
			struct megasas_evtarg_pd pd;
			u32 prevState;
			u32 newState;
		} __attribute__ ((packed)) pd_state;

		struct {
			u16 vendorId;
			u16 deviceId;
			u16 subVendorId;
			u16 subDeviceId;
		} __attribute__ ((packed)) pci;

		u32 rate;
		char str[96];

		struct {
			u32 rtc;
			u32 elapsedSeconds;
		} __attribute__ ((packed)) time;

		struct {
			u32 ecar;
			u32 elog;
			char str[64];
		} __attribute__ ((packed)) ecc;

		u8 b[96];
		u16 s[48];
		u32 w[24];
		u64 d[12];
	} args;

	char description[128];

} __attribute__ ((packed));

#define MIN(a,b) ((a)<(b) ? (a):(b))
typedef void (*XOR_LOW_LEVEL_GEN_FUNC)(u32 **, u32);
typedef u8 (*XOR_LOW_LEVEL_CHECK_FUNC)(u32 **, u32);

/*
 * enumerates type of descriptor
 */
typedef  enum _MR_CPX_DESCRIPTOR_TYPE {
	MR_CPX_DESCRIPTOR_TYPE_COPY   = 1,
	MR_CPX_DESCRIPTOR_TYPE_XOR    = 2
} MR_CPX_DESCRIPTOR_TYPE;

/*
 * status information of copy or xor operation
 */
typedef enum _MR_CPX_STATUS {
	MR_CPX_STATUS_SUCCESS      = 0,
	MR_CPX_STATUS_INCONSISTENT = 1,
	MR_CPX_STATUS_FAILURE      = 2,
} MR_CPX_STATUS;     

/*
 * define the XOR opcodes
 */
typedef enum _mr_cpx_xor_op {
	MR_CPX_XOR_OP_GEN_P    = 0x01,                  // generate P buffer
	MR_CPX_XOR_OP_GEN_Q    = 0x02,                  // generate Q buffer
	MR_CPX_XOR_OP_GEN_PQ   = 0x03,                  // generate P+Q buffers
	MR_CPX_XOR_OP_CHECK_P  = 0x11,                  // check P buffer (and generate if bad)
	MR_CPX_XOR_OP_CHECK_Q  = 0x12,                  // check Q buffer (and generate if bad)
	MR_CPX_XOR_OP_CHECK_PQ = 0x13,                  // check P+Q buffers (and generate if bad)
} MR_CPX_XOR_OP;

#define MR_CPX_XOR_OP_IS_CHECK(xorOp)  ((xorOp & 0x10)!=0)              // TRUE if operation is a CHECK operation
#define MR_CPX_XOR_OP_IS_GEN(xorOp)    (!MR_CPX_XOR_OP_IS_CHECK(xorOp)) // TRUE if operation is a GEN operation
#define MR_CPX_XOR_OP_IS_P(xorOp)      ((xorOp & 0x01)!=0)              // TRUE if operation is for P or P/Q
#define MR_CPX_XOR_OP_IS_Q(xorOp)      ((xorOp & 0x02)!=0)              // TRUE if operation is for Q or P/Q
#define MR_CPX_XOR_OP_IS_PQ(xorOp)     ((xorOp & 0x03)==3)              // TRUE if operation is for P/Q

/*
 * this data is passed to driver during driver init.
 */
struct mr_cpx_init_data {
	u32	cpx_desc_count;		// Number of cpx desc required by fw.
	u32     size;                   // size of the buffer
	u64     phys_addr_cache_buf;       // physical address of cache buffer allocated by pre-boot

} __attribute__ ((packed));

/*
 * header passed with each descriptor
 */
struct mr_cpx_header {
	u32        context   : 24;       // context information passed by firmware, to be passed back in response data
	u32        type     :  4;       // type of descriptor
	u32        resvd    :  4;
} __attribute__ ((packed));

/*
 * xor descriptor data
 */
struct mr_cpx_xor_descriptor {
	struct mr_cpx_header      hdr;
	MR_CPX_XOR_OP      op;                           // xor operation for gen/check of p/q/p+q
	u32             size;                         // number of bytes to gen/check for this operation
	u32             buff_valid_bitmap;              // bitmap of valid buffers for input
	u8              p_idx;                       // index of p buffer within list (for p/pq gen/check functions)
	u8              q_idx;                       // index of q buffer within list (for q/pq gen/check functions)
	u8              pad[2];             
	u32             buff_list[MAX_MR_ROW_SIZE];    // list of buffers for this xor operation (32 bit offset)
	u32             mult_list[MAX_MR_ROW_SIZE];    // list of coefficient multipliers for q operations
} __attribute__ ((packed));

/*
 * copy buffer for each transfer. each such tranfer between
 * user spare host address and firmware allocated cache data.
 */
struct mr_cpx_copy_mr_buffer {
	u32     buf;                       // buffer address/offset 
	u32     size;                      // size of copy 
} __attribute__ ((packed));

/*
 * copy descriptor data
 */
struct mr_cpx_copy_descriptor {
	struct mr_cpx_header              hdr;
	u32                     mfi_cmd_cxt;          // mfi context
	u32                     total_byte_count;         // total byte count for this transfer 
	u32                     host_skip_count;          // skip count from starting address of host buffer 
	u8                      dir;                    // direction of transfer 
	u8                      pad[3];
	struct mr_cpx_copy_mr_buffer      copy_buf[MAX_MR_ROW_SIZE];
} __attribute__ ((packed)) ;

/*
 * users of this interface must allocate memory for the size of
 * this structure while allocating memory for descriptors
 */
union mr_cpx_descriptor {
	struct mr_cpx_xor_descriptor       cpx_xor_desc;
	struct mr_cpx_copy_descriptor     cpx_copy_desc;
	u8                      pad[512];
} __attribute__ ((packed));

/*
 * request queue.
 * firmware manages producerindex, driver manages consumerindex.
 * number of decriptors is kept as variable. driver must use
 * max host commands supported for allocation.
 */
struct mr_cpx_request_queue {
	u32             consumer_idx;
	u32             producer_idx;
	union mr_cpx_descriptor   cpxdescriptor[1]; // use max host commands 
} __attribute__ ((packed));

/*
 * response data. this data will be posted by driver after copy/xor
 * operation is compete.
 */
union mr_cpx_response_data {
	struct {
		u32         context     : 24;                // context  
		u32         status      :  4;                // status in the form of cpx_status
		u32         type        :  4;
	} r;
	u32         w;
} __attribute__ ((packed));

/*
 * response queue.
 * driver manages producerindex, firmware manages consumerindex.
 * number of decriptors is kept as variable. driver must use
 * max host commands supported for allocation.
 */
struct  mr_cpx_response_queue {
	u32                 consumer_idx;
	u32                 producer_idx;
	union mr_cpx_response_data   cpx_resp_data[1]; // use max host commands 
} __attribute__ ((packed));

/*
 * the size of each of the structure within this is determined at run time.
 * this structure is for document purpose and shows that the structures
 * lay as shown below in memory
 */
struct  mr_cpx_queues {
	struct mr_cpx_request_queue     requestqueue;
	struct mr_cpx_response_queue    responsequeue;
} __attribute__ ((packed));

/*
 * driver sends this queue data during mfi init. firmware
 * will not use the interface if the versions do not match.
 */
struct mr_cpx_queue_data {
	u32         version;
	u32         count_queue_entries;
	u64         phys_addr_cpx_queues;
} __attribute__ ((packed));

struct megasas_aen_event {
	struct delayed_work hotplug_work;
	struct megasas_instance *instance;
};

#define MAX_PERF_COLLECTION_VD  MAX_LOGICAL_DRIVES_EXT
#define BLOCKTOMB_BITSHIFT      11

/*                                                                              
 * defines the logical drive performance metrics structure                      
 * These metrics are valid for the current collection period                    
 */

typedef struct _MR_IO_METRICS_SIZE {
    u32         lessThan512B;                       // Number of IOs: size <= 512B                                                                             
    u32         between512B_4K;                     // Number of IOs: 512B < size <=4K                                                                         
    u32         between4K_16K;                      // Number of IOs:   4K < size <=16K                                                                        
    u32         between16K_64K;                     // Number of IOs:  16K < size <=64K                                                                        
    u32         between64K_256K;                    // Number of IOs:  64K < size <=256K                                                                       
    u32         moreThan256K;                       // Number of IOs: 256K < size                                                                              
} __attribute__ ((packed)) MR_IO_METRICS_SIZE;

/*                                                                              
 * define the structure to capture the randomness of the IOs                    
 * each counter is for IOs, whose LBA is set distance apart from the previous I\
O.                                                                              
 */
typedef struct _MR_IO_METRICS_RANDOMNESS {
    u32         sequential;                         // Number of IOs: sequential ( inter-LBA distance is 0)                                                    
    u32         lessThan64K;                        // Number of IOs: within 64KB of previous IO                                                               
    u32         between64K_512K;                    // Number of IOs:  64K < LBA <=512K                                                                        
    u32         between512K_16M;                    // Number of IOs: 512K < LBA <=16M                                                                         
    u32         between16M_256M;                    // Number of IOs:  16M < LBA <=256M                                                                        
    u32         between256M_1G;                     // Number of IOs: 256M < LBA <=1G                                                                          
    u32         moreThan1G;                         // Number of IOs:   1G < LBA                                                                               
} __attribute__ ((packed)) MR_IO_METRICS_RANDOMNESS;

/*                                                                              
 * define the structure for LD cache usage                                      
 */
typedef struct _MR_IO_METRICS_LD_CACHE {
    u8                          targetId;
    u8                          reserved[7];        // For future use           
    MR_IO_METRICS_SIZE          readSizeCache;      // Reads to Primary Cache   
    MR_IO_METRICS_SIZE          writeSizeCache;     // Writes to Primary Cache  
    MR_IO_METRICS_SIZE          readSizeSSC;        // Reads to Secondary Cache
    MR_IO_METRICS_SIZE          writeSizeSSC;       // Writes to Secondary Cache                                                                               
} __attribute__ ((packed)) MR_IO_METRICS_LD_CACHE;

/*                                                                              
 * define the structure for controller cache usage                              
 */
typedef struct _MR_IO_METRICS_CACHE {
    u32                         size;               // size of this data structure (including size field)                                                      
    u32                         collectionPeriod;   // Time (sec), taken to collect this data                                                                  

    u32                         avgDirtyCache;      // Running average of dirty cache (% of cache size)                                                        
    u32                         avgCacheUsed;       // Running average of total cache in use                                                                   
    u32                         readAheadCache;     // Cache(MB) used for Read Ahead data                                                                      
    u32                         readAheadSSC;       // Secondary Cache(MB) used for Read Ahead data                                                            
    u32                         unusedReadAheadCache;   // Cache(MB) for Read Ahead data, that wasn't accessed                                                 
    u32                         unusedReadAheadSSC; // Secondary Cache(MB) for Read Ahead data, that wasn't accessed                                           
    u32                         flushBlockTime;     // Time(ms) IOs were blocked while cache is flushed etc.                                                   

    u8                          reserved[2];        // For future use           
    u16                         count;              // count of number of targetId entries in this list                                                        
    MR_IO_METRICS_LD_CACHE      ldIoCache[1];       // Variable list of LD IO metrics                                                                          
} __attribute__ ((packed)) MR_IO_METRICS_CACHE;

/*                                                                              
 * define the structure for overall LD IO metrics (from host perspective)       
 */
typedef struct _MR_IO_METRICS_LD_OVERALL {
    u8                          targetId;
    u8                          pad;
    u16                         idleTime;           // Total seconds, LD has been idle                                                                         
    u32                         reserved;
    u32                         readMB;             // Total read data transferred in MB                                                                       
    u32                         writeMB;            // Total write data transferred in MB                                                                      
    MR_IO_METRICS_SIZE          readSize;           // Aggregagate the number of read IOs for total IO count                                                   
    MR_IO_METRICS_SIZE          writeSize;          // Aggregate the number write IOs for write total IO count                                                 
    MR_IO_METRICS_RANDOMNESS    readRandomness;
    MR_IO_METRICS_RANDOMNESS    writeRandomness;
} __attribute__ ((packed)) MR_IO_METRICS_LD_OVERALL;

typedef struct _MR_IO_METRICS_LD_OVERALL_LIST {
    u32                         size;               // size of this data structure (including size field)                                                      
    u32                         collectionPeriod;   // Time (sec), taken to collect this data                                                                  

    MR_IO_METRICS_LD_OVERALL    ldIOOverall[1];     // Variable list of overall LD IO metrics                                                                  
} __attribute__ ((packed)) MR_IO_METRICS_LD_OVERALL_LIST;

/*                                                                              
 * define the structure for controller's IO metrics                             
 */
typedef struct _MR_IO_METRICS {
    MR_IO_METRICS_CACHE             ctrlIoCache;    // controller cache usage   
    MR_IO_METRICS_LD_OVERALL_LIST   ldIoMetrics;    // overall host IO metrics  
} __attribute__ ((packed)) MR_IO_METRICS;

typedef struct _PERFORMANCEMETRIC
{
    u8                          LogOn;
    MR_IO_METRICS_LD_OVERALL    IoMetricsLD[MAX_PERF_COLLECTION_VD];
    MR_IO_METRICS_LD_OVERALL    SavedIoMetricsLD[MAX_PERF_COLLECTION_VD];
    u64                         LastBlock[MAX_PERF_COLLECTION_VD];
    u64                         LastIOTime[MAX_PERF_COLLECTION_VD];
    u64                         CollectEndTime;
    u64                         CollectStartTime;
    u32                         SavedCollectTimeSecs;
}PERFORMANCEMETRIC;

struct megasas_irq_context {
	struct megasas_instance *instance;
	u32 MSIxIndex;
};

/*
 *  define the structure specific to drive order feature
 */
struct megasas_drive_order {
	u8 needs_ordering;         /* Drive ordering feature is enabled/disabled */
	u8 boot_drive_is_pd;       /* Set if boot device is PD */
	u16 boot_fw_tgt_id;        /* FW Target ID */
	u8 boot_drive_channel_id;  /* OS Channel ID */
	u16 boot_drive_tgt_id;     /* OS Target ID */
};

enum MEGASAS_OCR_CAUSE {
	FW_FAULT_OCR			= 0,
	SCSIIO_TIMEOUT_OCR		= 1,
	MFI_IO_TIMEOUT_OCR		= 2,
};

enum DCMD_RETURN_STATUS {
	DCMD_SUCCESS		= 0,
	DCMD_TIMEOUT		= 1,
	DCMD_FAILED		= 2,
	DCMD_NOT_FIRED		= 3,
};

typedef struct _MR_DRV_SYSTEM_INFO
{
	u8	infoVersion;      
	u8	systemIdLength;
	u16	reserved0;
	u8	systemId[64];
	u8	reserved[1980];
} MR_DRV_SYSTEM_INFO, *PMR_DRV_SYSTEM_INFO;

#define MR_DRV_SYSTEM_INFO_SIZE sizeof(MR_DRV_SYSTEM_INFO)


typedef enum _MR_PD_TYPE {
	UNKNOWN_DRIVE = 0,
	PARALLEL_SCSI = 1,
	SAS_PD = 2,
	SATA_PD = 3,
	FC_PD = 4,
	NVME_PD = 5,
} MR_PD_TYPE;

/* JBOD Queue depth defintions */
#define MEGASAS_SATA_QD	32
#define MEGASAS_SAS_QD	64
#define MEGASAS_NVME_QD	32	/*TBD, when FW provides interface, this needs to be changed*/
#define MEGASAS_DEFAULT_PD_QD	64

#define MR_DEFAULT_NVME_PAGE_SIZE	4096
#define MR_DEFAULT_NVME_PAGE_SHIFT	12
#define MR_DEFAULT_NVME_MDTS_KB		128

struct megasas_instance {

	u32 *producer;
	dma_addr_t producer_h;
	u32 *consumer;
	dma_addr_t consumer_h;
	u32 *verbuf;
	dma_addr_t verbuf_h;
	PMR_DRV_SYSTEM_INFO system_info_buf;
	dma_addr_t system_info_h;
	struct MR_LD_VF_AFFILIATION *vf_affiliation;
	dma_addr_t vf_affiliation_h;
	struct MR_LD_VF_AFFILIATION_111 *vf_affiliation_111;
	dma_addr_t vf_affiliation_111_h;
	struct MR_CTRL_HB_HOST_MEM *hb_host_mem;
	dma_addr_t hb_host_mem_h;

	struct MR_PD_INFO *pd_info;
	dma_addr_t pd_info_h;

	struct MR_TARGET_PROPERTIES *tgt_prop;
	dma_addr_t tgt_prop_h;

	u32 *reply_queue;
	dma_addr_t reply_queue_h;

	u32 *crash_dump_buf;
	dma_addr_t crash_dump_h;

	struct MR_PD_LIST *pd_list_buf;
	dma_addr_t pd_list_buf_h;

	struct megasas_ctrl_info *ctrl_info_buf;
	dma_addr_t ctrl_info_buf_h;

	struct MR_LD_LIST *ld_list_buf;
	dma_addr_t ld_list_buf_h;

	struct MR_LD_TARGETID_LIST *ld_targetid_list_buf;
	dma_addr_t ld_targetid_list_buf_h;

	void *crash_buf[MAX_CRASH_DUMP_SIZE];
	unsigned int    fw_crash_buffer_size;
	unsigned int    fw_crash_state;
        unsigned int    fw_crash_buffer_offset;

	resource_size_t base_addr;
	struct megasas_register_set __iomem *reg_set;
	u32 *reply_post_host_index_addr[MR_MAX_MSIX_REG_ARRAY];
	struct megasas_pd_list          pd_list[MEGASAS_MAX_PD];
	struct megasas_pd_list          local_pd_list[MEGASAS_MAX_PD];
	u8     ld_ids[MEGASAS_MAX_LD_IDS];

	s8 init_id;

	u16 max_num_sge;
	u16 max_fw_cmds;
	u16 max_mpt_cmds;
	u16 cur_can_queue;
	u16 max_scsi_cmds;
	/* For Fusion its num IOCTL cmds, for others MFI based its max_fw_cmds */
	u16 max_mfi_cmds;
	u16 ldio_threshold;
	u32 max_sectors_per_req;
	struct megasas_aen_event *ev;

	struct megasas_cmd **cmd_list;
	struct list_head cmd_pool;
	spinlock_t cmd_pool_lock;
	spinlock_t hba_lock;
	spinlock_t stream_lock;
	/* used to synch producer, consumer ptrs in dpc */
	spinlock_t completion_lock;

	struct dma_pool *frame_dma_pool;
	struct dma_pool *sense_dma_pool;

	struct megasas_evt_detail *evt_detail;
	dma_addr_t evt_detail_h;
	struct megasas_cmd *aen_cmd;
	struct semaphore ioctl_sem;

	struct Scsi_Host *host;

	wait_queue_head_t int_cmd_wait_q;
	wait_queue_head_t abort_cmd_wait_q;

	struct pci_dev *pdev;
	u32 unique_id;
	u32 fw_support_ieee;

	atomic_t fw_outstanding;
	atomic_t ldio_outstanding;
	atomic_t fw_reset_no_pci_access;
	atomic_t ieee_sgl;
	atomic_t prp_sgl;
	atomic_t r1_fp_writes_count;
	atomic_t sge_holes_type1;
	atomic_t sge_holes_type2;
	atomic_t sge_holes_type3;

	struct megasas_instance_template *instancet;
	struct tasklet_struct isr_tasklet;
	struct work_struct work_init;
	struct work_struct crash_init;

	u8 flag;
	u8 unload;
	u8 flag_ieee;
	u8 issuepend_done;
	u8 disableOnlineCtrlReset;
	u8 UnevenSpanSupport;
	
	u8 supportmax256vd;
 	u8 pd_list_not_supported;
	
	u16 fw_supported_vd_count;
	u16 fw_supported_pd_count;
	
	u16 drv_supported_vd_count;
	u16 drv_supported_pd_count;

	atomic_t adprecovery;
	unsigned long last_time;
	u32 mfiStatus;
	u32 last_seq_num;

	struct list_head internal_reset_pending_q;

        u32 cpx_supported;
        struct mr_cpx_request_queue *cpx_request_queue;
        dma_addr_t cpx_request_queue_h;
        union mr_cpx_descriptor *cpx_dscrptr;
        u32 cpx_dscrptr_cnt;
        u64 host_mem_phys;
        u32 host_mem_len;
        u8 *host_mem_virt;
        u32 drv_buf_index;	/* driver maiantined index of copied host buffers*/
        u32 drv_buf_alloc;	/*Allocated host crash buffers*/
        u32 crash_dump_fw_support;
        u32 crash_dump_drv_support; /* End to end support for crash dump*/
        u32 crash_dump_app_support;
	u32 secure_jbod_support;   /* FW support for Secure JBOD*/
	u32 support_morethan256jbod;            /* FW suport for more than 256 PD/JBOD */
	bool use_seqnum_jbod_fp;   /* Added for PD sequence */
	spinlock_t crashdump_lock;

	/* Ptr to hba specfic information */
	void *ctrl_context;
	u32 ctrl_context_pages;
	unsigned int msix_vectors;
	struct msix_entry msixentry[MEGASAS_MAX_MSIX_QUEUES];
	struct megasas_irq_context irq_context[MEGASAS_MAX_MSIX_QUEUES];
	u64 map_id;
	u64 pd_seq_map_id;
	struct megasas_cmd *map_update_cmd;
	struct megasas_cmd *jbod_seq_cmd;
	unsigned long bar;
	long reset_flags;
	PERFORMANCEMETRIC PerformanceMetric;
	u32 CurLdCount;
	struct mutex reset_mutex;
	struct timer_list sriov_heartbeat_timer;
	char skip_heartbeat_timer_del;
	u8 requestorId;
	char PlasmaFW111;
 	char clusterId[MEGASAS_CLUSTER_ID_SIZE];
 	u8 peerIsPresent;
 	u8 passive;
	u16 throttlequeuedepth;
	u8 is_imr;
	u8 is_rdpq;
	u8 mask_interrupts;
	u16 max_chain_frame_sz;
	bool dev_handle;
	struct megasas_drive_order drive_order;
    bool fw_sync_cache_support;
    u32 mfi_frame_size;
 	bool msix_combined;
 	u16 maxRaidMapSize;
	/* preffered count to send as LDIO irrspective of FP capable.*/
	u8  r1_ldio_hint_default;
	u32 nvme_page_size;
	u8 adapter_type;
	bool consistent_mask_64bit;
	bool support_nvme_passthru;
};

struct MR_LD_VF_MAP {
	u32 size;
	MR_LD_REF ref;
	u8 ldVfCount;
	u8 reserved[6];
	u8 policy[1];
};

struct MR_LD_VF_AFFILIATION {
	u32 size;
	u8 ldCount;
	u8 vfCount;
	u8 thisVf;
	u8 reserved[9];
	struct MR_LD_VF_MAP map[1];
};

/* Plasma 1.11 FW backward compatibility structures */
#define IOV_111_OFFSET 0x7CE
#define MAX_VIRTUAL_FUNCTIONS 8
#define MR_LD_ACCESS_HIDDEN 15

struct IOV_111 {
	u8 maxVFsSupported;
	u8 numVFsEnabled;
	u8 requestorId;
	u8 reserved[5];
};

struct MR_LD_VF_MAP_111 {
	u8 targetId;
	u8 reserved[3];
	u8 policy[MAX_VIRTUAL_FUNCTIONS];
};

struct MR_LD_VF_AFFILIATION_111 {
	u8 vdCount;
	u8 vfCount;
	u8 thisVf;
	u8 reserved[5];
	struct MR_LD_VF_MAP_111 map[MAX_LOGICAL_DRIVES];
};

struct MR_CTRL_HB_HOST_MEM {
	struct {
		u32 fwCounter;	/* Firmware heart beat counter */
		struct {
			u32 debugmode:1; /* 1=Firmware is in debug mode.
					    Heart beat will not be updated. */
			u32 reserved:31;
		} debug;
		u32 reserved_fw[6];
		u32 driverCounter; /* Driver heart beat counter.  0x20 */
		u32 reserved_driver[7];
	} HB;
	u8 pad[0x400-0x40];
};

enum {
	MEGASAS_HBA_OPERATIONAL			= 0,
	MEGASAS_ADPRESET_SM_INFAULT		= 1,
	MEGASAS_ADPRESET_SM_FW_RESET_SUCCESS	= 2,
	MEGASAS_ADPRESET_SM_OPERATIONAL		= 3,
	MEGASAS_HW_CRITICAL_ERROR		= 4,
	MEGASAS_ADPRESET_SM_POLLING		= 5,
	MEGASAS_ADPRESET_INPROG_SIGN		= 0xDEADDEAD,
};


struct megasas_instance_template {
	void (*fire_cmd)(struct megasas_instance *, dma_addr_t, \
		u32, struct megasas_register_set __iomem *);

	void (*enable_intr)(struct megasas_instance *) ;
	void (*disable_intr)(struct megasas_instance *);

	int (*clear_intr)(struct megasas_register_set __iomem *);

	u32 (*read_fw_status_reg)(struct megasas_register_set __iomem *);
	int (*adp_reset)(struct megasas_instance *, struct megasas_register_set __iomem *);
	int (*check_reset)(struct megasas_instance *, struct megasas_register_set __iomem *);

	//irqreturn_t (*service_isr )(int irq, void *devp, struct pt_regs *regs);
	irqreturn_t (*service_isr )(int irq, void *devp);
	void (*tasklet)(unsigned long);
	u32 (*init_adapter)(struct megasas_instance *);
	u32 (*build_and_issue_cmd) (struct megasas_instance *, struct scsi_cmnd *);
	void (*issue_dcmd) (struct megasas_instance *instance,
				struct megasas_cmd *cmd);
};

#define MEGASAS_IS_LOGICAL(sdev)						\
	((sdev->channel < MEGASAS_MAX_PD_CHANNELS) ? 0 : 1)

#define MEGASAS_DEV_INDEX(scp)						\
	(((scp->device->channel % 2) * MEGASAS_MAX_DEV_PER_CHANNEL) + 	\
	scp->device->id)

#define MEGASAS_PD_INDEX(scp)						\
	((scp->device->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +		\
	scp->device->id)

struct megasas_cmd {

	union megasas_frame *frame;
	dma_addr_t frame_phys_addr;
	u8 *sense;
	dma_addr_t sense_phys_addr;

	u32 index;
	u8 sync_cmd;
	u8 cmd_status_drv;
	u8 abort_aen;
        u8 retry_for_fw_reset;
	u8 flags;

	struct list_head list;
	struct scsi_cmnd *scmd;

	struct megasas_instance *instance;
	union {
		struct {
			u16 smid;
			u16 resvd;
		} context;
		u32 frame_count;
	};
};

#define MAX_MGMT_ADAPTERS		1024
#define MAX_IOCTL_SGE			16

struct megasas_iocpacket {

	u16 host_no;
	u16 __pad1;
	u32 sgl_off;
	u32 sge_count;
	u32 sense_off;
	u32 sense_len;
	union {
		u8 raw[128];
		struct megasas_header hdr;
	} frame;

	struct iovec sgl[MAX_IOCTL_SGE];

} __attribute__ ((packed));

struct megasas_aen {
	u16 host_no;
	u16 __pad1;
	u32 seq_num;
	u32 class_locale_word;
} __attribute__ ((packed));

#ifdef CONFIG_COMPAT
struct compat_megasas_iocpacket {
	u16 host_no;
	u16 __pad1;
	u32 sgl_off;
	u32 sge_count;
	u32 sense_off;
	u32 sense_len;
	union {
		u8 raw[128];
		struct megasas_header hdr;
	} frame;
	struct compat_iovec sgl[MAX_IOCTL_SGE];
} __attribute__ ((packed));

#define MEGASAS_IOC_FIRMWARE32	_IOWR('M', 1, struct compat_megasas_iocpacket)
#endif

#define MEGASAS_IOC_FIRMWARE	_IOWR('M', 1, struct megasas_iocpacket)
#define MEGASAS_IOC_GET_AEN	_IOW('M', 3, struct megasas_aen)

struct megasas_mgmt_info {

	u16 count;
	struct megasas_instance *instance[MAX_MGMT_ADAPTERS];
	int max_index;
};

u8
MR_BuildRaidContext(struct megasas_instance *instance,
		    struct IO_REQUEST_INFO *io_info,
		    RAID_CONTEXT *pRAID_Context,
		    MR_DRV_RAID_MAP_ALL *map, u8 **raidLUN);
u16 MR_TargetIdToLdGet(u32 ldTgtId, MR_DRV_RAID_MAP_ALL *map);
MR_LD_RAID *MR_LdRaidGet(u32 ld, MR_DRV_RAID_MAP_ALL *map);
u16 MR_ArPdGet(u32 ar, u32 arm, MR_DRV_RAID_MAP_ALL *map);
u16 MR_LdSpanArrayGet(u32 ld, u32 span, MR_DRV_RAID_MAP_ALL *map);
u16 MR_PdDevHandleGet(u32 pd, MR_DRV_RAID_MAP_ALL *map);
u16 MR_GetLDTgtId(u32 ld, MR_DRV_RAID_MAP_ALL *map);

int megasas_reset_fusion(struct Scsi_Host *shost, u8 reason);
int megasas_get_ctrl_info(struct megasas_instance *instance);
/* PD sequence */
int
megasas_sync_pd_seq_num(struct megasas_instance *instance, bool pend);
void megasas_set_dynamic_target_properties(struct scsi_device *sdev);

int megasas_task_abort_fusion(struct scsi_cmnd *scmd);
int megasas_reset_target_fusion(struct scsi_cmnd *scmd);
u32 mega_mod64(u64 dividend, u32 divisor);
void megasas_set_dma_settings(struct megasas_instance *instance,
	struct megasas_dcmd_frame *dcmd, dma_addr_t dma_addr, u32 dma_len);

#define msi_control_reg(base) (base + PCI_MSI_FLAGS)

#if !PCI_MSIX_FLAGS_ENABLE
	#define PCI_MSIX_FLAGS_ENABLE (1 << 15)
#endif

#endif				/*LSI_MEGARAID_SAS_H */
