/*
 *  Linux MegaRAID driver for SAS based RAID controllers
 *
 *  Copyright (c) 2003-2017  LSI Corporation.
 *  Copyright (c) 2003-2017  Avago Technologies.
 *  
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
 *
 *  Authors: Avago Technologies 
 *           Sreenivas Bagalkote
 *           Sumant Patro
 *           Bo Yang
 *           Adam Radford 
 *
 *  Send feedback to: <mr-driverindiadev.pdl@broadcom.com>
 *
 *     ATTN: Linuxraid
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uio.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/poll.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_dbg.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))
#include <linux/smp_lock.h>
#endif

#include "megaraid_sas_fusion.h"
#include "megaraid_sas.h"

/*
 * Number of sectors per IO command
 * Will be set in megasas_init_mfi if user does not provide
 */
static unsigned int max_sectors;
module_param_named(max_sectors, max_sectors, int, 0);
MODULE_PARM_DESC(max_sectors,
       "Maximum number of sectors per IO command");

static int msix_disable;
module_param(msix_disable, int, S_IRUGO);
MODULE_PARM_DESC(msix_disable, "Disable MSI-X interrupt handling. Default: 0");

static int smp_affinity_enable = 1;
module_param(smp_affinity_enable, int, S_IRUGO);
MODULE_PARM_DESC(smp_affinity_enable, "SMP affinity feature enable/disbale Default: enable(1)");

static unsigned int msix_vectors;
module_param(msix_vectors, int, S_IRUGO);
MODULE_PARM_DESC(msix_vectors, "MSI-X max vector count. Default: Set by FW");

static int allow_vf_ioctls;
module_param(allow_vf_ioctls, int, S_IRUGO);
MODULE_PARM_DESC(allow_vf_ioctls, "Allow ioctls in SR-IOV VF mode. Default: 0");

static unsigned int throttlequeuedepth = MEGASAS_THROTTLE_QUEUE_DEPTH;
module_param(throttlequeuedepth, int, S_IRUGO);
MODULE_PARM_DESC(throttlequeuedepth, "Adapter queue depth when throttled due to I/O timeout. Default: 16");

unsigned int resetwaittime = MEGASAS_RESET_WAIT_TIME;
module_param(resetwaittime, int, S_IRUGO);
MODULE_PARM_DESC(resetwaittime, "Wait time in (0-180s) after I/O timeout before resetting adapter. Default: 180s");

unsigned int scmd_timeout = MEGASAS_DEFAULT_CMD_TIMEOUT;
module_param(scmd_timeout, int, S_IRUGO);
MODULE_PARM_DESC(scmd_timeout, "scsi command timeout (10-90s), default 90s. See megasas_reset_timer.");

int crashdump_enable = 1;
module_param(crashdump_enable, int, S_IRUGO);
MODULE_PARM_DESC(crashdump_enable, "Firmware Crash dump feature enable/disbale Default: enable(1)");

int rdpq_enable = 1;
module_param(rdpq_enable, int, S_IRUGO);
MODULE_PARM_DESC(rdpq_enable, " Allocate reply queue in chunks for large queue depth enable/disable Default: enable(1)");

unsigned int dual_qdepth_disable;
module_param(dual_qdepth_disable, int, S_IRUGO);
MODULE_PARM_DESC(dual_qdepth_disable, "Disable dual queue depth feature. Default: 0");

MODULE_LICENSE("GPL");
MODULE_VERSION(MEGASAS_VERSION);
MODULE_AUTHOR("megaraidlinux.pdl@avagotech.com");
MODULE_DESCRIPTION("Avago MegaRAID SAS Driver");

/*
 * PCI ID table for all supported controllers
 */
static struct pci_device_id megasas_pci_table[] = {

	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1064R)},
	/* xscale IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078R)},
	/* ppc IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078DE)},
	/* ppc IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS1078GEN2)},
	/* gen2*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0079GEN2)},
	/* gen2*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0073SKINNY)},
	/* skinny*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_SAS0071SKINNY)},
	/* skinny*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_VERDE_ZCR)},
	/* xscale IOP, vega */
	{PCI_DEVICE(PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_PERC5)},
	/* xscale IOP */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_FUSION)},
	/* Fusion */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_PLASMA)},
	/* Plasma */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_INVADER)},
	/* Invader */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_FURY)},
	/* Fury */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_INTRUDER)},
	/* Intruder */
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_INTRUDER_24)},
	/* Intruder 24 port*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_CUTLASS_52)},
	/* iMR Intruder*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_CUTLASS_53)},
	/*VENTURA*/
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_VENTURA)},
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_HARPOON)},
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_TOMCAT)},
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_VENTURA_4PORT)},
	{PCI_DEVICE(PCI_VENDOR_ID_LSI_LOGIC, PCI_DEVICE_ID_LSI_CRUSADER_4PORT)},
	{}
};

MODULE_DEVICE_TABLE(pci, megasas_pci_table);

static int megasas_mgmt_majorno;
struct megasas_mgmt_info megasas_mgmt_info;
static struct fasync_struct *megasas_async_queue;
static DEFINE_MUTEX(megasas_async_queue_mutex);

static struct workqueue_struct *megasas_ocr_wq;
static int megasas_poll_wait_aen;
static DECLARE_WAIT_QUEUE_HEAD(megasas_poll_wait);
static u32 support_poll_for_event;
static u32 support_device_change;

/* define lock for aen poll */
spinlock_t poll_aen_lock;

u32 megasas_dbg_lvl;

static int
megasas_get_pd_info(struct megasas_instance *instance, u16 device_id);
int
megasas_set_crash_dump_params(struct megasas_instance *instance, u8 crash_buf_state);
void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status);

int megasas_transition_to_ready(struct megasas_instance* instance, int ocr);
static int megasas_get_pd_list(struct megasas_instance *instance);
static int megasas_ld_list_query(struct megasas_instance *instance,
 				 u8 query_type);
static int megasas_issue_init_mfi(struct megasas_instance *instance);
static int megasas_register_aen(struct megasas_instance *instance, u32 seq_num, u32 class_locale_word);
static int megasas_check_cpx_support( struct megasas_instance *instance);
static u32 megasas_remove_cpx( struct megasas_instance *instance);
static int megasas_send_cpx_queue_data( struct megasas_instance *instance );
int megasas_handle_cpx_requests( struct megasas_instance *instance);
static u32 megasas_read_fw_status_reg_gen2(struct megasas_register_set __iomem * regs);
static int megasas_adp_reset_gen2(struct megasas_instance *instance, struct megasas_register_set __iomem * reg_set);
static irqreturn_t megasas_isr(int irq, void *devp);
static u32
megasas_init_adapter_mfi(struct megasas_instance *instance);
u32
megasas_build_and_issue_cmd(struct megasas_instance *instance, struct scsi_cmnd *scmd);
static void megasas_complete_cmd_dpc(unsigned long instance_addr);
void
megasas_release_fusion(struct megasas_instance *instance);
int
megasas_ioc_init_fusion(struct megasas_instance *instance);
void
megasas_free_cmds_fusion(struct megasas_instance *instance);
u8
megasas_get_map_info(struct megasas_instance *instance);
int
megasas_sync_map_info(struct megasas_instance *instance);
int
wait_and_poll(struct megasas_instance *instance, struct megasas_cmd *cmd,
	int seconds);
void  megasas_reset_reply_desc(struct megasas_instance *instance);
u8 MR_ValidateMapInfo(struct megasas_instance *instance);
void megasas_fusion_ocr_wq(struct work_struct *work);
static int megasas_get_ld_vf_affiliation(struct megasas_instance *instance,
					 int initial);
int megasas_check_mpio_paths(struct megasas_instance *instance,
			     struct scsi_cmnd *scmd);
void megasas_fusion_crash_dump_wq(struct work_struct *work);
extern void megasas_free_host_crash_buffer
		(struct megasas_instance *instance);
extern void
megasas_return_cmd_fusion(struct megasas_instance *instance, struct megasas_cmd_fusion *cmd);
static int
megasas_setup_irqs(struct megasas_instance *instance, u8 is_probe);
extern int
megasas_alloc_fusion_context(struct megasas_instance *instance);
extern void
megasas_free_fusion_context(struct megasas_instance *instance);

int
megasas_issue_dcmd(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	instance->instancet->fire_cmd(instance,
		cmd->frame_phys_addr, 0, instance->reg_set);

	return 0;
}

/**
 * megasas_get_cmd -	Get a command from the free pool
 * @instance:		Adapter soft state
 *
 * Returns a free command from the pool
 */
struct megasas_cmd *megasas_get_cmd(struct megasas_instance
						  *instance)
{
	unsigned long flags;
	struct megasas_cmd *cmd = NULL;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	if (!list_empty(&instance->cmd_pool)) {
		cmd = list_entry((&instance->cmd_pool)->next,
				 struct megasas_cmd, list);
		list_del_init(&cmd->list);
	} else {
		printk(KERN_ERR "megasas: Command pool empty!\n");
	}

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
	return cmd;
}

/**
 * megasas_return_cmd -	Return a cmd to free command pool
 * @instance:		Adapter soft state
 * @cmd:		Command packet to be returned to free command pool
 */
inline void
megasas_return_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;
	u32 blk_tags;
	struct megasas_cmd_fusion *cmd_fusion;
	struct fusion_context *fusion = instance->ctrl_context;

	/* This flag is used only for fusion adapter.
	 * Wait for Interrupt for Polled mode DCMD
	 */
	if (cmd->flags & DRV_DCMD_POLLED_MODE)
		return;

	spin_lock_irqsave(&instance->cmd_pool_lock, flags);

	if (fusion) {
		blk_tags = instance->max_scsi_cmds + cmd->index;
#if BLK_TAG_DEBUG
		dev_info(&instance->pdev->dev,
			"%s : opcode (0x%08x) tag (%04d) cmd flags (0x%08x)\n",
				__func__, le32_to_cpu(cmd->frame->dcmd.opcode),
				blk_tags, cmd->flags);
#endif
		cmd_fusion = fusion->cmd_list[blk_tags];
		megasas_return_cmd_fusion(instance, cmd_fusion);
	}
	cmd->scmd = NULL;
	cmd->frame_count = 0;
	cmd->flags = 0;
	if (!fusion && reset_devices)
		cmd->frame->hdr.cmd = MFI_CMD_INVALID;
	list_add(&cmd->list, (&instance->cmd_pool)->next);

	spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);

}

static const char *
format_timestamp(uint32_t timestamp)
{
 	static char buffer[32];
 
 	if ((timestamp & 0xff000000) == 0xff000000)
 		snprintf(buffer, sizeof(buffer), "boot + %us", timestamp &
 		0x00ffffff);
 	else
 		snprintf(buffer, sizeof(buffer), "%us", timestamp);
 	return (buffer);
}
 
 
static const char *
format_class(int8_t class)
{
 	static char buffer[6];
 
 	switch (class) {
 	case MFI_EVT_CLASS_DEBUG:
 		return ("debug");
 	case MFI_EVT_CLASS_PROGRESS:
 		return ("progress");
 	case MFI_EVT_CLASS_INFO:
 		return ("info");
 	case MFI_EVT_CLASS_WARNING:
 		return ("WARN");
 	case MFI_EVT_CLASS_CRITICAL:
 		return ("CRIT");
 	case MFI_EVT_CLASS_FATAL:
 		return ("FATAL");
 	case MFI_EVT_CLASS_DEAD:
 		return ("DEAD");
 	default:
 		snprintf(buffer, sizeof(buffer), "%d", class);
 		return (buffer);
 	}
}
 
/**
  * megasas_decode_evt: Decode FW AEN event and print critical event
  * for information.
  * @instance:			Adapter soft state
  */
static void
megasas_decode_evt(struct megasas_instance *instance)
{
	struct megasas_evt_detail *evt_detail = instance->evt_detail;
	union megasas_evt_class_locale class_locale;
	class_locale.word = le32_to_cpu(evt_detail->cl.word);

	if (class_locale.members.class >= MFI_EVT_CLASS_CRITICAL)
		dev_info(&instance->pdev->dev, "%d (%s/0x%04x/%s) - %s\n",
			le32_to_cpu(evt_detail->seq_num),
			format_timestamp(le32_to_cpu(evt_detail->time_stamp)),
			(class_locale.members.locale),
			format_class(class_locale.members.class),
			evt_detail->description);
}
 
/**
*	The following functions are defined for xscale 
*	(deviceid : 1064R, PERC5) controllers
*/

/**
 * megasas_enable_intr_xscale -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_xscale(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	regs = instance->reg_set;
	writel(0, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_xscale -Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_xscale(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0x1f;
	regs = instance->reg_set;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_xscale - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_xscale(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_msg_0);
}
/**
 * megasas_clear_interrupt_xscale -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_xscale(struct megasas_register_set __iomem * regs)
{
	u32 status;
	u32 mfiStatus = 0;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (status & MFI_OB_INTR_STATUS_MASK)
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;
	if (status & MFI_XSCALE_OMR0_CHANGE_INTERRUPT)
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;

	/*
	 * Clear the interrupt by writing back the same value
	 */
	if (mfiStatus)
	writel(status, &regs->outbound_intr_status);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_status);

	return mfiStatus;
}

/**
 * megasas_fire_cmd_xscale -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_xscale(struct megasas_instance *instance,
		dma_addr_t frame_phys_addr,
		u32 frame_count,
		struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr >> 3)|(frame_count),
	       &(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

/**
 * megasas_adp_reset_xscale -	For controller reset
 * @regs:				MFI register set
 */
static int 
megasas_adp_reset_xscale(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
        u32 i;
	u32 pcidata;
	writel(MFI_ADP_RESET, &regs->inbound_doorbell);

	for (i=0; i < 3; i++)
		msleep(1000); /* sleep for 3 secs */
	pcidata =0;
	pci_read_config_dword(instance->pdev, MFI_1068_PCSR_OFFSET, &pcidata);
	printk("pcidata = %x\n", pcidata);
	if (pcidata & 0x2) {
		printk("mfi 1068 offset read=%x\n", pcidata);
		pcidata &= ~0x2;
		pci_write_config_dword(instance->pdev, MFI_1068_PCSR_OFFSET, pcidata);

		for (i=0; i<2; i++)
			msleep(1000); /* need to wait 2 secs again */

		pcidata =0;
		pci_read_config_dword(instance->pdev, MFI_1068_FW_HANDSHAKE_OFFSET, &pcidata);
		printk("mfi 1068 offset handshake read=%x\n", pcidata);
		if ((pcidata & 0xffff0000) == MFI_1068_FW_READY) {
			printk("mfi 1068 offset handshake=%x\n", pcidata);
			pcidata = 0;
			pci_write_config_dword(instance->pdev, MFI_1068_FW_HANDSHAKE_OFFSET, pcidata);
		}
	}
	return 0;
}

/**
 * megasas_check_reset_xscale -	For controller reset check
 * @regs:				MFI register set
 */
static int 
megasas_check_reset_xscale(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{

	if ((atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL) && 
	    (le32_to_cpu(*instance->consumer) ==
		MEGASAS_ADPRESET_INPROG_SIGN))
		return 1;

	return 0;
}

static struct megasas_instance_template megasas_instance_template_xscale = {

	.fire_cmd = megasas_fire_cmd_xscale,
	.enable_intr = megasas_enable_intr_xscale,
	.disable_intr = megasas_disable_intr_xscale,
	.clear_intr = megasas_clear_intr_xscale,
	.read_fw_status_reg = megasas_read_fw_status_reg_xscale,
	.adp_reset = megasas_adp_reset_xscale,
	.check_reset = megasas_check_reset_xscale,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

/**
*	This is the end of set of functions & definitions specific 
*	to xscale (deviceid : 1064R, PERC5) controllers
*/

/**
*	The following functions are defined for ppc (deviceid : 0x60) 
* 	controllers
*/

/**
 * megasas_enable_intr_ppc -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_ppc(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	regs = instance->reg_set;
	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);
    
	writel(~0x80000000, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_ppc -	Disable interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_ppc(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0xFFFFFFFF;
	regs = instance->reg_set;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_ppc - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_ppc(struct megasas_register_set __iomem * regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_ppc -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int 
megasas_clear_intr_ppc(struct megasas_register_set __iomem * regs)
{
	u32 status, mfiStatus = 0;

	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (status & MFI_REPLY_1078_MESSAGE_INTERRUPT)
		mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;

	if (status & MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT)
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_doorbell_clear);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_doorbell_clear);

	return mfiStatus;
}

/**
 * megasas_fire_cmd_ppc -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void 
megasas_fire_cmd_ppc(struct megasas_instance *instance,
		dma_addr_t frame_phys_addr,
		u32 frame_count,
		struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr | (frame_count<<1))|1, 
			&(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

/**
 * megasas_check_reset_ppc -	For controller reset check
 * @regs:				MFI register set
 */
static int 
megasas_check_reset_ppc(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
	if (atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL)
		return 1;

	return 0;
}

static struct megasas_instance_template megasas_instance_template_ppc = {
	
	.fire_cmd = megasas_fire_cmd_ppc,
	.enable_intr = megasas_enable_intr_ppc,
	.disable_intr = megasas_disable_intr_ppc,
	.clear_intr = megasas_clear_intr_ppc,
	.read_fw_status_reg = megasas_read_fw_status_reg_ppc,
	.adp_reset = megasas_adp_reset_xscale,
	.check_reset = megasas_check_reset_ppc,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

/**
 * megasas_enable_intr_skinny -	Enables interrupts
 * @regs:			MFI register set
 */
static inline void
megasas_enable_intr_skinny(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	regs = instance->reg_set;
	writel(0xFFFFFFFF, &(regs)->outbound_intr_mask);

	/* write ~0x00000005 (4 & 1) to the intr mask*/
	writel(~MFI_SKINNY_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_skinny -	Disables interrupt
 * @regs:			MFI register set
 */
static inline void
megasas_disable_intr_skinny(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0xFFFFFFFF;
	regs = instance->reg_set;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_skinny - returns the current FW status value
 * @regs:			MFI register set
 */
static u32
megasas_read_fw_status_reg_skinny(struct megasas_register_set __iomem *regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_skinny -	Check & clear interrupt
 * @regs:				MFI register set
 */
static int
megasas_clear_intr_skinny(struct megasas_register_set __iomem *regs)
{
	u32 status;
	u32 mfiStatus = 0;

	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

	if (!(status & MFI_SKINNY_ENABLE_INTERRUPT_MASK)) {
		return 0;
	}

	/*
	 * Check if it is our interrupt
	 */
	if ((megasas_read_fw_status_reg_gen2( regs) & MFI_STATE_MASK ) ==
	    MFI_STATE_FAULT ) {
		mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;
	} else
		mfiStatus |=  MFI_INTR_FLAG_REPLY_MESSAGE;

	/*
	 * Clear the interrupt by writing back the same value
	 */
	writel(status, &regs->outbound_intr_status);

	/*
	* dummy read to flush PCI
	*/
	readl(&regs->outbound_intr_status);

	return mfiStatus;
}

/**
 * megasas_fire_cmd_skinny -	Sends command to the FW
 * @frame_phys_addr :		Physical address of cmd
 * @frame_count :		Number of frames for the command
 * @regs :			MFI register set
 */
static inline void
megasas_fire_cmd_skinny(struct megasas_instance *instance,
			dma_addr_t frame_phys_addr,
			u32 frame_count,
			struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel(upper_32_bits(frame_phys_addr),
	       &(regs)->inbound_high_queue_port);
	writel((lower_32_bits(frame_phys_addr) | (frame_count<<1))|1,
	       &(regs)->inbound_low_queue_port);
	mmiowb();
	spin_unlock_irqrestore(&instance->hba_lock, flags);
	/*msleep(5);*/
}

/**
 * megasas_check_reset_skinny -	For controller reset check
 * @regs:				MFI register set
 */
static int 
megasas_check_reset_skinny(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
	if (atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL)
		return 1;

	return 0;
}

static struct megasas_instance_template megasas_instance_template_skinny = {

	.fire_cmd = megasas_fire_cmd_skinny,
	.enable_intr = megasas_enable_intr_skinny,
	.disable_intr = megasas_disable_intr_skinny,
	.clear_intr = megasas_clear_intr_skinny,
	.read_fw_status_reg = megasas_read_fw_status_reg_skinny,
	.adp_reset = megasas_adp_reset_gen2,
	.check_reset = megasas_check_reset_skinny,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

int disable_ext_io;
module_param(disable_ext_io, int, S_IRUGO);
MODULE_PARM_DESC(disable_ext_io, "Disable 1M IO support Default: 0(1M IO enabled)");

/**
*	The following functions are defined for gen2 (deviceid : 0x78 0x79)
*	controllers
*/

/**
 * megasas_enable_intr_gen2 -  Enables interrupts
 * @regs:                      MFI register set
 */
static inline void
megasas_enable_intr_gen2(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	regs = instance->reg_set;
	writel(0xFFFFFFFF, &(regs)->outbound_doorbell_clear);

	/* write ~0x00000005 (4 & 1) to the intr mask*/
	writel(~MFI_GEN2_ENABLE_INTERRUPT_MASK, &(regs)->outbound_intr_mask);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_disable_intr_gen2 - Disables interrupt
 * @regs:                      MFI register set
 */
static inline void
megasas_disable_intr_gen2(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *regs;
	u32 mask = 0xFFFFFFFF;
	regs = instance->reg_set;
	writel(mask, &regs->outbound_intr_mask);
	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_mask);
}

/**
 * megasas_read_fw_status_reg_gen2 - returns the current FW status value
 * @regs:                      MFI register set
 */
static u32
megasas_read_fw_status_reg_gen2(struct megasas_register_set __iomem *regs)
{
	return readl(&(regs)->outbound_scratch_pad);
}

/**
 * megasas_clear_interrupt_gen2 -      Check & clear interrupt
 * @regs:                              MFI register set
 */
static int
megasas_clear_intr_gen2(struct megasas_register_set __iomem *regs)
{
	u32 status;
        u32 mfiStatus = 0;
	/*
	 * Check if it is our interrupt
	 */
	status = readl(&regs->outbound_intr_status);

        if (status & MFI_INTR_FLAG_REPLY_MESSAGE)
        {
                mfiStatus = MFI_INTR_FLAG_REPLY_MESSAGE;
        }
        if (status & MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT)
        {
                mfiStatus |= MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE;
        }

	/*
	 * Clear the interrupt by writing back the same value
	 */
        if (mfiStatus)
	writel(status, &regs->outbound_doorbell_clear);

	/* Dummy readl to force pci flush */
	readl(&regs->outbound_intr_status);

        return mfiStatus;

}
/**
 * megasas_fire_cmd_gen2 -     Sends command to the FW
 * @frame_phys_addr :          Physical address of cmd
 * @frame_count :              Number of frames for the command
 * @regs :                     MFI register set
 */
static inline void
megasas_fire_cmd_gen2(struct megasas_instance *instance,
			dma_addr_t frame_phys_addr,
			u32 frame_count,
			struct megasas_register_set __iomem *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&instance->hba_lock, flags);
	writel((frame_phys_addr | (frame_count<<1))|1,
			&(regs)->inbound_queue_port);
	spin_unlock_irqrestore(&instance->hba_lock, flags);
}

/**
 * megasas_adp_reset_gen2 -	For controller reset
 * @regs:				MFI register set
 */
static int 
megasas_adp_reset_gen2(struct megasas_instance *instance, struct megasas_register_set __iomem * reg_set)
{
	u32			retry = 0, delay = 0;
	u32			HostDiag;
	u32                     *seq_offset = &reg_set->seq_offset;
	u32                     *hostdiag_offset = &reg_set->host_diag;

        if ( instance->instancet ==  &megasas_instance_template_skinny ){
                seq_offset = &reg_set->fusion_seq_offset;
                hostdiag_offset = &reg_set->fusion_host_diag;
        }

        writel(0, seq_offset);
        writel(4, seq_offset);
        writel(0xb, seq_offset);
        writel(2, seq_offset);
        writel(7, seq_offset);
        writel(0xd, seq_offset);
	
	msleep(1000);

        HostDiag = (u32)readl(hostdiag_offset);

	while ( !( HostDiag & DIAG_WRITE_ENABLE) )
	{
		msleep(100);
                HostDiag = (u32)readl(hostdiag_offset);
		dev_info(&instance->pdev->dev, "%s:%d ADP_RESET_GEN2: retry time=0x%x, hostdiag=0x%08x\n", 
			__func__, __LINE__, retry, HostDiag);
		
		if (retry++ >= 100) 
			return 1;

}

	dev_info(&instance->pdev->dev, "%s:%d ADP_RESET_GEN2: HostDiag=0x%08x\n",
		__func__, __LINE__, HostDiag);

        writel((HostDiag | DIAG_RESET_ADAPTER), hostdiag_offset);

	for (delay=0; delay<10; delay++) 
	{
		msleep(1000);
	}
	
        HostDiag = (u32)readl(hostdiag_offset);
	while ( ( HostDiag & DIAG_RESET_ADAPTER) )
	{
		msleep(100);
                HostDiag = (u32)readl(hostdiag_offset);
		dev_info(&instance->pdev->dev, "%s:%d ADP_RESET_GEN2: retry time=0x%x, hostdiag=0x%08x\n", 
			__func__, __LINE__, retry, HostDiag);
		
		if (retry++ >= 1000) 
			return 1;

	}
	return 0;
}

/**
 * megasas_check_reset_gen2 -	For controller reset check
 * @regs:				MFI register set
 */
static int 
megasas_check_reset_gen2(struct megasas_instance *instance, struct megasas_register_set __iomem * regs)
{
	if (atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL)
		return 1;

	return 0;
}

static struct megasas_instance_template megasas_instance_template_gen2 = {

	.fire_cmd = megasas_fire_cmd_gen2,
	.enable_intr = megasas_enable_intr_gen2,
	.disable_intr = megasas_disable_intr_gen2,
	.clear_intr = megasas_clear_intr_gen2,
	.read_fw_status_reg = megasas_read_fw_status_reg_gen2,
	.adp_reset = megasas_adp_reset_gen2,
	.check_reset = megasas_check_reset_gen2,
	.service_isr = megasas_isr,
	.tasklet = megasas_complete_cmd_dpc,
	.init_adapter = megasas_init_adapter_mfi,
	.build_and_issue_cmd = megasas_build_and_issue_cmd,
	.issue_dcmd = megasas_issue_dcmd,
};

/**
*	This is the end of set of functions & definitions
 *      specific to gen2 (deviceid : 0x78, 0x79) controllers
*/
/*
 * Template added for TB (Fusion)
 */
extern struct megasas_instance_template megasas_instance_template_fusion;

/**
 * megasas_issue_polled -	Issues a polling command
 * @instance:			Adapter soft state
 * @cmd:			Command packet to be issued 
 *
 * For polling, MFI requires the cmd_status to be set to MFI_STAT_INVALID_STATUS before posting.
 */
int
megasas_issue_polled(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	struct megasas_header *frame_hdr = &cmd->frame->hdr;

	frame_hdr->cmd_status = MFI_STAT_INVALID_STATUS;
	frame_hdr->flags |= cpu_to_le16(MFI_FRAME_DONT_POST_IN_REPLY_QUEUE);
	
	if ((atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR) ||
		(instance->instancet->issue_dcmd(instance, cmd))) {
		dev_err(&instance->pdev->dev, "Failed from %s %d\n",
			__func__, __LINE__);
		return DCMD_NOT_FIRED;
	}

	return wait_and_poll(instance, cmd, instance->requestorId ?
				MEGASAS_ROUTINE_WAIT_TIME_VF: MFI_IO_TIMEOUT_SECS);
}

/**
 * megasas_issue_blocked_cmd -	Synchronous wrapper around regular FW cmds
 * @instance:			Adapter soft state
 * @cmd:			Command to be issued
 * @timeout:			Timeout in seconds
 *
 * This function waits on an event for the command to be returned from ISR.
 * Max wait time is MEGASAS_INTERNAL_CMD_WAIT_TIME secs
 * Used to issue ioctl commands.
 */
int
megasas_issue_blocked_cmd(struct megasas_instance *instance,
			  struct megasas_cmd *cmd, int timeout)
{
	int ret = 0;
	cmd->cmd_status_drv = MFI_STAT_INVALID_STATUS;

	if ((atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR) ||
		(instance->instancet->issue_dcmd(instance, cmd))) {
		dev_err(&instance->pdev->dev, "Failed from %s %d\n",
			__func__, __LINE__);
		return DCMD_NOT_FIRED;
	}

	if (timeout) {
		ret = wait_event_timeout(instance->int_cmd_wait_q,
				cmd->cmd_status_drv != MFI_STAT_INVALID_STATUS, timeout * HZ);
		if (!ret) {
			dev_err(&instance->pdev->dev, "Failed from %s %d DCMD Timed out \n",
				__func__, __LINE__);
			return DCMD_TIMEOUT;
		}
	} else
		wait_event(instance->int_cmd_wait_q,
				cmd->cmd_status_drv != MFI_STAT_INVALID_STATUS);
		
	return (cmd->cmd_status_drv == MFI_STAT_OK) ?
		DCMD_SUCCESS : DCMD_FAILED;
}

/**
 * megasas_issue_blocked_abort_cmd -	Aborts previously issued cmd
 * @instance:				Adapter soft state
 * @cmd_to_abort:			Previously issued cmd to be aborted
 * @timeout:				Timeout in seconds
 *
 * MFI firmware can abort previously issued AEN comamnd (automatic event
 * notification). The megasas_issue_blocked_abort_cmd() issues such abort
 * cmd and waits for return status.
 * Max wait time is MEGASAS_INTERNAL_CMD_WAIT_TIME secs
 */
static int
megasas_issue_blocked_abort_cmd(struct megasas_instance *instance,
				struct megasas_cmd *cmd_to_abort, int timeout)
{
	struct megasas_cmd *cmd;
	struct megasas_abort_frame *abort_fr;
	int ret = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -1;

	abort_fr = &cmd->frame->abort;

	/*
	 * Prepare and issue the abort frame
	 */
	abort_fr->cmd = MFI_CMD_ABORT;
	abort_fr->cmd_status = MFI_STAT_INVALID_STATUS;
	abort_fr->flags = cpu_to_le16(0);
	abort_fr->abort_context = cpu_to_le32(cmd_to_abort->index);
	abort_fr->abort_mfi_phys_addr_lo =
		cpu_to_le32(lower_32_bits(cmd_to_abort->frame_phys_addr));
	abort_fr->abort_mfi_phys_addr_hi =
		cpu_to_le32(upper_32_bits(cmd_to_abort->frame_phys_addr));

	cmd->sync_cmd = 1;
	cmd->cmd_status_drv = MFI_STAT_INVALID_STATUS;

	if ((atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR) ||
		(instance->instancet->issue_dcmd(instance, cmd))) {
		dev_err(&instance->pdev->dev, "Failed from %s %d\n",
			__func__, __LINE__);
		return DCMD_NOT_FIRED;
	}
	
	if (timeout) {
		ret = wait_event_timeout(instance->abort_cmd_wait_q,
				cmd->cmd_status_drv != MFI_STAT_INVALID_STATUS, timeout * HZ);
		if (!ret) {
			dev_err(&instance->pdev->dev, "Failed from %s %d Abort Timed out \n",
				__func__, __LINE__);
			return DCMD_TIMEOUT;
		}
	}
	else
		wait_event(instance->abort_cmd_wait_q,
				cmd->cmd_status_drv != MFI_STAT_INVALID_STATUS);
	
	cmd->sync_cmd = 0;

	megasas_return_cmd(instance, cmd);
	return (cmd->cmd_status_drv == MFI_STAT_OK) ?
		DCMD_SUCCESS : DCMD_FAILED;
}

/**
 * megasas_make_sgl32 -	Prepares 32-bit SGL
 * @instance:		Adapter soft state
 * @scp:		SCSI command from the mid-layer
 * @mfi_sgl:		SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl32(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);
	BUG_ON(sge_count < 0);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge32[i].length = cpu_to_le32(sg_dma_len(os_sgl));
			mfi_sgl->sge32[i].phys_addr = cpu_to_le32(sg_dma_address(os_sgl));
		}
	}
	return sge_count;
}

/**
 * megasas_make_sgl64 -	Prepares 64-bit SGL
 * @instance:		Adapter soft state
 * @scp:		SCSI command from the mid-layer
 * @mfi_sgl:		SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl64(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);
	BUG_ON(sge_count < 0);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge64[i].length = cpu_to_le32(sg_dma_len(os_sgl));
			mfi_sgl->sge64[i].phys_addr = cpu_to_le64(sg_dma_address(os_sgl));
		}
	}
	return sge_count;
}

/**
 * megasas_make_sgl_skinny - Prepares IEEE SGL
 * @instance:           Adapter soft state
 * @scp:                SCSI command from the mid-layer
 * @mfi_sgl:            SGL to be filled in
 *
 * If successful, this function returns the number of SG elements. Otherwise,
 * it returnes -1.
 */
static int
megasas_make_sgl_skinny(struct megasas_instance *instance,
		struct scsi_cmnd *scp, union megasas_sgl *mfi_sgl)
{
	int i;
	int sge_count;
	struct scatterlist *os_sgl;

	sge_count = scsi_dma_map(scp);

	if (sge_count) {
		scsi_for_each_sg(scp, os_sgl, sge_count, i) {
			mfi_sgl->sge_skinny[i].length =
				cpu_to_le32(sg_dma_len(os_sgl));
			mfi_sgl->sge_skinny[i].phys_addr =
				cpu_to_le64(sg_dma_address(os_sgl));
			mfi_sgl->sge_skinny[i].flag = cpu_to_le32(0);
		}
	}
	return sge_count;
}

 /**
 * megasas_get_frame_count - Computes the number of frames
 * @frame_type		: type of frame- io or pthru frame
 * @sge_count		: number of sg elements
 *
 * Returns the number of frames required for numnber of sge's (sge_count)
 */

static u32 megasas_get_frame_count(struct megasas_instance *instance,
			u8 sge_count, u8 frame_type)
{
	int num_cnt;
	int sge_bytes;
	u32 sge_sz;
	u32 frame_count=0;

	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);
	}

	/*
	 * Main frame can contain 2 SGEs for 64-bit SGLs and
	 * 3 SGEs for 32-bit SGLs for ldio &
	 * 1 SGEs for 64-bit SGLs and
	 * 2 SGEs for 32-bit SGLs for pthru frame
	 */
	if (unlikely(frame_type == PTHRU_FRAME)) {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;
		} else if (IS_DMA64)
			num_cnt = sge_count - 1;
		else
			num_cnt = sge_count - 2;
	} else {
		if (instance->flag_ieee == 1) {
			num_cnt = sge_count - 1;
		} else if (IS_DMA64)
			num_cnt = sge_count - 2;
		else
			num_cnt = sge_count - 3;
	}

	if(num_cnt>0){
		sge_bytes = sge_sz * num_cnt;

		frame_count = (sge_bytes / MEGAMFI_FRAME_SIZE) +
		    ((sge_bytes % MEGAMFI_FRAME_SIZE) ? 1 : 0) ;
	}
	/* Main frame */
	frame_count +=1;

	if (frame_count > 7)
		frame_count = 8;
	return frame_count;
}

/**
 * megasas_build_dcdb -	Prepares a direct cdb (DCDB) command
 * @instance:		Adapter soft state
 * @scp:		SCSI command
 * @cmd:		Command to be prepared in
 *
 * This function prepares CDB commands. These are typcially pass-through
 * commands to the devices.
 */
static int
megasas_build_dcdb(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 is_logical;
	u32 device_id;
	u16 flags = 0;
	struct megasas_pthru_frame *pthru;

	is_logical = MEGASAS_IS_LOGICAL(scp);
	device_id = MEGASAS_DEV_INDEX(scp);
	pthru = (struct megasas_pthru_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;
	else if (scp->sc_data_direction == PCI_DMA_NONE)
		flags = MFI_FRAME_DIR_NONE;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}

	/*
	 * Prepare the DCDB frame
	 */
	pthru->cmd = (is_logical) ? MFI_CMD_LD_SCSI_IO : MFI_CMD_PD_SCSI_IO;
	pthru->cmd_status = 0x0;
	pthru->scsi_status = 0x0;
	pthru->target_id = device_id;
	pthru->lun = scp->device->lun;
	pthru->cdb_len = scp->cmd_len;
	pthru->timeout = 0;
	pthru->pad_0 = 0;
	pthru->flags = cpu_to_le16(flags);
	pthru->data_xfer_len = cpu_to_le32(scsi_bufflen(scp));

	memcpy(pthru->cdb, scp->cmnd, scp->cmd_len);

	/*
	* If the command is for the tape device, set the
	* pthru timeout to the os layer timeout value.
	*/
	if (scp->device->type == TYPE_TAPE) {
		if ((scp->request->timeout / HZ) > 0xFFFF)
			pthru->timeout = 0xFFFF;
		else
			pthru->timeout = cpu_to_le16(scp->request->timeout / HZ);
	}

	/*
	 * Construct SGL
	 */
	if (instance->flag_ieee == 1) {
		pthru->flags |= cpu_to_le16(MFI_FRAME_SGL64);
		pthru->sge_count = megasas_make_sgl_skinny(instance, scp,
						      &pthru->sgl);
	} else if (IS_DMA64) {
		pthru->flags |= cpu_to_le16(MFI_FRAME_SGL64);
		pthru->sge_count = megasas_make_sgl64(instance, scp,
						      &pthru->sgl);
	} else
		pthru->sge_count = megasas_make_sgl32(instance, scp,
						      &pthru->sgl);

       if (pthru->sge_count > instance->max_num_sge) {
        	printk("megasas: build_dcdb error, two many SGE\n");
	        return 0;
	}
	/*
	 * Sense info specific
	 */
	pthru->sense_len = SCSI_SENSE_BUFFERSIZE;
	pthru->sense_buf_phys_addr_hi =
		cpu_to_le32(upper_32_bits(cmd->sense_phys_addr));
	pthru->sense_buf_phys_addr_lo =
		cpu_to_le32(lower_32_bits(cmd->sense_phys_addr));

	/*
	 * Compute the total number of frames this command consumes. FW uses
	 * this number to pull sufficient number of frames from host memory.
	 */
	cmd->frame_count = megasas_get_frame_count(instance, pthru->sge_count,
							PTHRU_FRAME);

	return cmd->frame_count;
}

static inline u64 megasas_time_to_usecs(struct timeval *tv)
{
        return ((tv->tv_sec*1000000) + tv->tv_usec);
}

/**
 * UpdateIOMetric -	Update IO matrix for to use as stastics
 * @instance:		Adapter soft state
 * @TargetId:		Target ID of Scsi Device associated with IO
 * @isRead:		is a Read or write command value
 * @startBlock:		Startblock of IO
 * @NumBlocks:		NumBlocks of IO
 * @sector_size:	sector size of storage device.
 *
 * This function stores IO stastics in internal data structures, which
 * can be passed to application to display IO stats.
 * This function will convert any block size IO into 512 byte style IOs to provide correct 
 * stastics event if IOs are for difference sector size Drives.
 */
inline void UpdateIOMetric(struct megasas_instance *instance, u16 TargetId, u8 isRead, u64 startBlock, u32 NumBlocks, 
				u32 sector_size)
{
    MR_IO_METRICS_SIZE          *pIOSizeMetric;
    MR_IO_METRICS_RANDOMNESS    *pIORandomMetric;
    struct timeval              current_time;

    if (!instance->PerformanceMetric.LogOn)
        return;

    if (TargetId >= MAX_PERF_COLLECTION_VD)
        return;

	/* Convert NumBlocks and startBlock from any sector size to 512 byte */
	NumBlocks = NumBlocks * (sector_size/512);
	startBlock = startBlock * (sector_size/512);
	
    if (isRead)
    {
        pIOSizeMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].readSize;
        pIORandomMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].readRandomness;
        instance->PerformanceMetric.IoMetricsLD[TargetId].readMB += NumBlocks; // lba to MB conversion happens later                                           
    }
    else
    {
        pIOSizeMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].writeSize;
        pIORandomMetric = &instance->PerformanceMetric.IoMetricsLD[TargetId].writeRandomness;
        instance->PerformanceMetric.IoMetricsLD[TargetId].writeMB += NumBlocks; // lba to MB conversion happens later                                          
    }

    do_gettimeofday(&current_time);
    instance->PerformanceMetric.LastIOTime[TargetId] = megasas_time_to_usecs(&current_time);

    if (NumBlocks  <= 1)
        pIOSizeMetric->lessThan512B++;


    else if (NumBlocks  <=  8)
        pIOSizeMetric->between512B_4K++;
    else if (NumBlocks  <=  32)
        pIOSizeMetric->between4K_16K++;
    else if (NumBlocks  <=  128)
        pIOSizeMetric->between16K_64K++;
    else if (NumBlocks  <=  256)
        pIOSizeMetric->between64K_256K++;
    else if (NumBlocks  >  256)
        pIOSizeMetric->moreThan256K++;                        // Number of IOs: 256K < size                                                                    
    if (instance->PerformanceMetric.LastBlock[TargetId] == startBlock)
        pIORandomMetric->sequential++;                         // Number of IOs: sequential ( inter-LBA distance is 0)                                         
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 128 ) <= startBlock)
        pIORandomMetric->lessThan64K++;                        // Number of IOs: within 64KB of previous IO                                                    
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 1024 ) <= startBlock)
        pIORandomMetric->between64K_512K++;                    // Number of IOs:  64K < LBA <=512K                                                             
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 32768) <= startBlock)
        pIORandomMetric->between512K_16M++;                    // Number of IOs: 512K < LBA <=16M                                                              
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 524288) <= startBlock)
        pIORandomMetric->between16M_256M++;                    // Number of IOs:  16M < LBA <=256M                                                             
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 2097152) <= startBlock)
        pIORandomMetric->between256M_1G++;                     // Number of IOs: 256M < LBA <=1G                                                               
    else if ((instance->PerformanceMetric.LastBlock[TargetId] + 2097152) > startBlock)
        pIORandomMetric->moreThan1G++;                         // Number of IOs:   1G < LBA                                                                    

    instance->PerformanceMetric.LastBlock[TargetId] = startBlock + NumBlocks;

}

static void CopyPerfMetricData(struct megasas_instance *instance, u64 CurrentTime)
{
	u16 i;
	u64 temp;

	// Block to MB conversion & idle time calculation.                         
	for (i=0; i <MAX_PERF_COLLECTION_VD; i++ ) {
		instance->PerformanceMetric.IoMetricsLD[i].readMB  >>= BLOCKTOMB_BITSHIFT;
		instance->PerformanceMetric.IoMetricsLD[i].writeMB >>= BLOCKTOMB_BITSHIFT;
		instance->PerformanceMetric.IoMetricsLD[i].targetId = i;
		temp = CurrentTime - instance->PerformanceMetric.LastIOTime[i];
		do_div(temp, 1000000);
		instance->PerformanceMetric.IoMetricsLD[i].idleTime += (u16)temp;
	}
	temp = CurrentTime - instance->PerformanceMetric.CollectStartTime;
	do_div(temp, 1000000);
	instance->PerformanceMetric.SavedCollectTimeSecs = (u32)temp;

	memcpy(instance->PerformanceMetric.SavedIoMetricsLD, instance->PerformanceMetric.IoMetricsLD, sizeof(MR_IO_METRICS_LD_OVERALL) * MAX_PERF_COLLECTION_VD);

	// Zero out the original buffer for new collection.                        
	memset(instance->PerformanceMetric.IoMetricsLD, 0, sizeof(MR_IO_METRICS_LD_OVERALL) * MAX_PERF_COLLECTION_VD);
}

static void ProcessPerfMetricAEN(struct megasas_instance *instance, MR_CTRL_IO_METRICS_CMD_TYPE MetricType)
{
    struct timeval      current_time;
    u16 i;

    do_gettimeofday(&current_time);

    switch (MetricType) {
    case MR_CTRL_IO_METRICS_CMD_STOP:
        if (instance->PerformanceMetric.LogOn) {

            // Set the flag to Stop collection.                                 
            instance->PerformanceMetric.LogOn = 0;

            CopyPerfMetricData(instance, megasas_time_to_usecs(&current_time));
        }
        break;

    case MR_CTRL_IO_METRICS_CMD_START:

        // Save the previously collected data.
	    if (instance->PerformanceMetric.LogOn)
		    CopyPerfMetricData(instance, megasas_time_to_usecs(&current_time));

        //Initialize the timers.                                                
        instance->PerformanceMetric.CollectStartTime = megasas_time_to_usecs(&current_time);
        for (i=0;i <MAX_PERF_COLLECTION_VD ; i++ )
            instance->PerformanceMetric.LastIOTime[i]  = megasas_time_to_usecs(&current_time);

        // Set the flag to start collection.                                    
        instance->PerformanceMetric.LogOn = 1;

        break;
    default:
        printk("megaraid_sas: unepxected value %x for metric type\n", MetricType);
        break;
    }
}

static void ProcessPerfMetricRequest(struct megasas_instance *instance, u8* ioctlBuffer, u32 dataTransferlength)
{
        MR_IO_METRICS *pIOMetric = (MR_IO_METRICS *)ioctlBuffer;
        MR_IO_METRICS_LD_OVERALL_LIST   *pldIoMetrics = (MR_IO_METRICS_LD_OVERALL_LIST *)(ioctlBuffer + pIOMetric->ctrlIoCache.size);

	if (instance->CurLdCount) {
		pldIoMetrics->size = (sizeof(MR_IO_METRICS_LD_OVERALL)*(min((u32)MAX_PERF_COLLECTION_VD, instance->CurLdCount) -1) + sizeof(MR_IO_METRICS_LD_OVERALL_LIST));
		if ((pldIoMetrics->size + pIOMetric->ctrlIoCache.size) <= dataTransferlength)
			memcpy(&pldIoMetrics->ldIOOverall[0], instance->PerformanceMetric.SavedIoMetricsLD, sizeof(MR_IO_METRICS_LD_OVERALL)*min((u32)MAX_PERF_COLLECTION_VD, instance->CurLdCount));
	}
	pldIoMetrics->collectionPeriod = instance->PerformanceMetric.SavedCollectTimeSecs;
}

/**
 * megasas_build_ldio -	Prepares IOs to logical devices
 * @instance:		Adapter soft state
 * @scp:		SCSI command
 * @cmd:		Command to be prepared
 *
 * Frames (and accompanying SGLs) for regular SCSI IOs use this function.
 */
static int
megasas_build_ldio(struct megasas_instance *instance, struct scsi_cmnd *scp,
		   struct megasas_cmd *cmd)
{
	u32 device_id;
	u8 sc = scp->cmnd[0];
	u16 flags = 0;
	struct megasas_io_frame *ldio;
	unsigned long spinlock_flags;
	u64 lba;

	device_id = MEGASAS_DEV_INDEX(scp);
	ldio = (struct megasas_io_frame *)cmd->frame;

	if (scp->sc_data_direction == PCI_DMA_TODEVICE)
		flags = MFI_FRAME_DIR_WRITE;
	else if (scp->sc_data_direction == PCI_DMA_FROMDEVICE)
		flags = MFI_FRAME_DIR_READ;

	if (instance->flag_ieee == 1) {
		flags |= MFI_FRAME_IEEE;
	}

	/*
	 * Prepare the Logical IO frame: 2nd bit is zero for all read cmds
	 */
	ldio->cmd = (sc & 0x02) ? MFI_CMD_LD_WRITE : MFI_CMD_LD_READ;
	ldio->cmd_status = 0x0;
	ldio->scsi_status = 0x0;
	ldio->target_id = device_id;
	ldio->timeout = 0;
	ldio->reserved_0 = 0;
	ldio->pad_0 = 0;
	ldio->flags = cpu_to_le16(flags);
	ldio->start_lba_hi = 0;
	ldio->access_byte = (scp->cmd_len != 6) ? scp->cmnd[1] : 0;

	/*
	 * 6-byte READ(0x08) or WRITE(0x0A) cdb
	 */
	if (scp->cmd_len == 6) {
		ldio->lba_count = cpu_to_le32((u32) scp->cmnd[4]);
		ldio->start_lba_lo = cpu_to_le32(((u32) scp->cmnd[1] << 16) |
						 ((u32) scp->cmnd[2] << 8) |
						 (u32) scp->cmnd[3]);

		ldio->start_lba_lo &= cpu_to_le32(0x1FFFFF);
	}

	/*
	 * 10-byte READ(0x28) or WRITE(0x2A) cdb
	 */
	else if (scp->cmd_len == 10) {
		ldio->lba_count = cpu_to_le32((u32) scp->cmnd[8] |
					      ((u32) scp->cmnd[7] << 8));
		ldio->start_lba_lo = cpu_to_le32(((u32) scp->cmnd[2] << 24) |
						 ((u32) scp->cmnd[3] << 16) |
						 ((u32) scp->cmnd[4] << 8) |
						 (u32) scp->cmnd[5]);
	}

	/*
	 * 12-byte READ(0xA8) or WRITE(0xAA) cdb
	 */
	else if (scp->cmd_len == 12) {
		ldio->lba_count = cpu_to_le32(((u32) scp->cmnd[6] << 24) |
					      ((u32) scp->cmnd[7] << 16) |
					      ((u32) scp->cmnd[8] << 8) |
					      (u32) scp->cmnd[9]);

		ldio->start_lba_lo = cpu_to_le32(((u32) scp->cmnd[2] << 24) |
						 ((u32) scp->cmnd[3] << 16) |
						 ((u32) scp->cmnd[4] << 8) |
						 (u32) scp->cmnd[5]);
	}

	/*
	 * 16-byte READ(0x88) or WRITE(0x8A) cdb
	 */
	else if (scp->cmd_len == 16) {
		ldio->lba_count = cpu_to_le32(((u32) scp->cmnd[10] << 24) |
					      ((u32) scp->cmnd[11] << 16) |
					      ((u32) scp->cmnd[12] << 8) |
					      (u32) scp->cmnd[13]);

		ldio->start_lba_lo = cpu_to_le32(((u32) scp->cmnd[6] << 24) |
						 ((u32) scp->cmnd[7] << 16) |
						 ((u32) scp->cmnd[8] << 8) |
						 (u32) scp->cmnd[9]);

		ldio->start_lba_hi = cpu_to_le32(((u32) scp->cmnd[2] << 24) |
						 ((u32) scp->cmnd[3] << 16) |
						 ((u32) scp->cmnd[4] << 8) |
						 (u32) scp->cmnd[5]);

	}

	/*
	 * Construct SGL
	 */
	if (instance->flag_ieee) {
		ldio->flags |= cpu_to_le16(MFI_FRAME_SGL64);
		ldio->sge_count = megasas_make_sgl_skinny(instance, scp,
					      &ldio->sgl);
	} else if (IS_DMA64) {
		ldio->flags |= cpu_to_le16(MFI_FRAME_SGL64);
		ldio->sge_count = megasas_make_sgl64(instance, scp, &ldio->sgl);
	} else
		ldio->sge_count = megasas_make_sgl32(instance, scp, &ldio->sgl);

        if (ldio->sge_count > instance->max_num_sge) {
        	printk("megasas: build_ld_io  error, sge_count = %x\n", ldio->sge_count);
	        return 0;
	}
	/*
	 * Sense info specific
	 */
	ldio->sense_len = SCSI_SENSE_BUFFERSIZE;
	ldio->sense_buf_phys_addr_hi = 0;
	ldio->sense_buf_phys_addr_lo = cpu_to_le32(cmd->sense_phys_addr);

	/*
	 * Compute the total number of frames this command consumes. FW uses
	 * this number to pull sufficient number of frames from host memory.
	 */
	cmd->frame_count = megasas_get_frame_count(instance,
			ldio->sge_count, IO_FRAME);

        lba     = (u64)ldio->start_lba_hi << 32 | ldio->start_lba_lo;
        spin_lock_irqsave(&instance->hba_lock, spinlock_flags);
        UpdateIOMetric(instance, device_id, ldio->cmd == MFI_CMD_LD_READ ? 1 : 0, lba, ldio->lba_count,
			scp->device->sector_size);
        spin_unlock_irqrestore(&instance->hba_lock, spinlock_flags);

	return cmd->frame_count;
}

/**
 * megasas_cmd_type -		Checks if the cmd is for logical drive/sysPD
 * 				and whether it's RW or non RW
 * @scmd:			SCSI command
 * Called by megasas_queue_command to find out if the command to be queued
 * is a logical drive command	
 */
inline int megasas_cmd_type(struct scsi_cmnd *cmd)
{
	int ret;
	
	switch (cmd->cmnd[0]) {
	case READ_10:
	case WRITE_10:
	case READ_12:
	case WRITE_12:
	case READ_6:
	case WRITE_6:
	case READ_16:
	case WRITE_16:
		ret = (MEGASAS_IS_LOGICAL(cmd)) ?
			READ_WRITE_LDIO : READ_WRITE_SYSPDIO;
		break;
	default:
		ret = (MEGASAS_IS_LOGICAL(cmd)) ?
			NON_READ_WRITE_LDIO : NON_READ_WRITE_SYSPDIO;
	}
	return ret;
}

 /**
 * megasas_dump_pending_frames -	Dumps the frame address of all pending cmds
 *                              	in FW
 * @instance:				Adapter soft state
 */
static inline void
megasas_dump_pending_frames(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	int i,n;
	union megasas_sgl *mfi_sgl;
	struct megasas_io_frame *ldio;
	struct megasas_pthru_frame *pthru;
	u32 sgcount;
	u32 max_cmd = instance->max_fw_cmds;

	printk(KERN_ERR "\nmegasas[%d]: Dumping Frame Phys Address of all pending cmds in FW\n",instance->host->host_no);
	printk(KERN_ERR "megasas[%d]: Total OS Pending cmds : %d\n",instance->host->host_no,atomic_read(&instance->fw_outstanding));
	if (IS_DMA64)
		printk(KERN_ERR "\nmegasas[%d]: 64 bit SGLs were sent to FW\n",instance->host->host_no);
	else
		printk(KERN_ERR "\nmegasas[%d]: 32 bit SGLs were sent to FW\n",instance->host->host_no);

	printk(KERN_ERR "megasas[%d]: Pending OS cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		if(!cmd->scmd)
			continue;
		printk(KERN_ERR "megasas[%d]: Frame addr :0x%08lx : ",instance->host->host_no,(unsigned long)cmd->frame_phys_addr);
		if (megasas_cmd_type(cmd->scmd) == READ_WRITE_LDIO){
			ldio = (struct megasas_io_frame *)cmd->frame;
			mfi_sgl = &ldio->sgl;
			sgcount = ldio->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x,"
			" lba lo : 0x%x, lba_hi : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",
			instance->host->host_no, cmd->frame_count, ldio->cmd, ldio->target_id,
			le32_to_cpu(ldio->start_lba_lo), le32_to_cpu(ldio->start_lba_hi),
			le32_to_cpu(ldio->sense_buf_phys_addr_lo), sgcount);
		}
		else {
			pthru = (struct megasas_pthru_frame *) cmd->frame;
			mfi_sgl = &pthru->sgl;
			sgcount = pthru->sge_count;
			printk(KERN_ERR "megasas[%d]: frame count : 0x%x, Cmd : 0x%x, Tgt id : 0x%x, "
			"lun : 0x%x, cdb_len : 0x%x, data xfer len : 0x%x, sense_buf addr : 0x%x,sge count : 0x%x\n",
			instance->host->host_no, cmd->frame_count, pthru->cmd, pthru->target_id,
			pthru->lun, pthru->cdb_len, le32_to_cpu(pthru->data_xfer_len),
			le32_to_cpu(pthru->sense_buf_phys_addr_lo), sgcount);
		}
	if(megasas_dbg_lvl & MEGASAS_DBG_LVL){
		for (n = 0; n < sgcount; n++){
			if (IS_DMA64)
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%llx ",
					le32_to_cpu(mfi_sgl->sge64[n].length),
					le64_to_cpu(mfi_sgl->sge64[n].phys_addr));
			else
				printk(KERN_ERR "megasas: sgl len : 0x%x, sgl addr : 0x%x ",
					le32_to_cpu(mfi_sgl->sge32[n].length),
					le32_to_cpu(mfi_sgl->sge32[n].phys_addr));
			}
		}
		printk(KERN_ERR "\n");
	} /*for max_cmd*/
	printk(KERN_ERR "\nmegasas[%d]: Pending Internal cmds in FW : \n",instance->host->host_no);
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if(cmd->sync_cmd == 1){
			printk(KERN_ERR "0x%08lx : ", (unsigned long)cmd->frame_phys_addr);
		}
	}
	printk(KERN_ERR "megasas[%d]: Dumping Done.\n\n",instance->host->host_no);
}

u32
megasas_build_and_issue_cmd(struct megasas_instance *instance, struct scsi_cmnd *scmd)
{
	struct megasas_cmd *cmd;
	u32 frame_count;
	

	cmd = megasas_get_cmd(instance);
	if (!cmd)
		return SCSI_MLQUEUE_HOST_BUSY;

	/*
	 * Logical drive command
	 */
	if (megasas_cmd_type(scmd) == READ_WRITE_LDIO)
		frame_count = megasas_build_ldio(instance, scmd, cmd);
	else
		frame_count = megasas_build_dcdb(instance, scmd, cmd);

	if (!frame_count) {
		goto out_return_cmd;
	}

	cmd->scmd = scmd;
	scmd->SCp.ptr = (char *)cmd;

	/*
	 * Issue the command to the FW
	 */
	atomic_inc(&instance->fw_outstanding);

	instance->instancet->fire_cmd(instance, cmd->frame_phys_addr,
				cmd->frame_count-1, instance->reg_set);

	return 0;
out_return_cmd:
	megasas_return_cmd(instance,cmd);
	return SCSI_MLQUEUE_HOST_BUSY;
}

/**
 * megasas_sync_cache_behavior - function to decide whether SYNC CACHE should
 *                               be sent to FW or not
 * @scmd:                        SCSI command to be queued
 * @return:                      SEND_TO_FW(0): Send SYNC CACHE down to firmware
 *                               DONOT_SEND_TO_FW(1): Don't send SYNC CACHE down to firmware
 */
inline bool megasas_sync_cache_behavior(struct scsi_cmnd *scmd)
{
	struct fusion_context *fusion;
	struct megasas_instance *instance;

	instance = (struct megasas_instance *)scmd->device->host->hostdata;
	fusion = instance->ctrl_context;

	if (instance->fw_sync_cache_support)
		return SEND_TO_FW;
	else
		return DONOT_SEND_TO_FW;
}

/**
 * megasas_queue_command -	Queue entry point
 * @scmd:			SCSI command to be queued
 * @done:			Callback entry point
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))
static int
megasas_queue_command(struct scsi_cmnd *scmd, void (*done) (struct scsi_cmnd *))
#else
static int
megasas_queue_command(struct Scsi_Host *shost, struct scsi_cmnd *scmd)
#endif
{
	struct megasas_instance *instance;
	struct MR_PRIV_DEVICE *mr_device_priv_data;

	instance = (struct megasas_instance *)
		scmd->device->host->hostdata;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))
	scmd->scsi_done = done;
#endif

	if (instance->unload == 1) {
		scsi_print_command(scmd);
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

        if (instance->issuepend_done == 0)
		return SCSI_MLQUEUE_HOST_BUSY;


	/* Check for an mpio path and adjust behavior */
	if (atomic_read(&instance->adprecovery) == MEGASAS_ADPRESET_SM_INFAULT) {
		if (megasas_check_mpio_paths(instance, scmd) ==
		    (DID_REQUEUE << 16))
			return SCSI_MLQUEUE_HOST_BUSY;
		else{
			scmd->result = DID_NO_CONNECT << 16;
			scmd->scsi_done(scmd);
			return 0;
		}
	}

	if (atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

	mr_device_priv_data = scmd->device->hostdata;
	if (!mr_device_priv_data) {
		scmd->result = DID_NO_CONNECT << 16;
		scmd->scsi_done(scmd);
		return 0;
	}

	//Don't process if we have already declared adapter dead
	// If we are in middle of bringing up the HBA, send the busy status to mid-layer
	// till the process is complete
	if (atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL)
		return SCSI_MLQUEUE_HOST_BUSY;
	else if (mr_device_priv_data->tm_busy)
		return SCSI_MLQUEUE_DEVICE_BUSY;


	scmd->result = 0;

	if (MEGASAS_IS_LOGICAL(scmd) &&
	    (scmd->device->id >= instance->fw_supported_vd_count || scmd->device->lun)) {
		scmd->result = DID_BAD_TARGET << 16;
		goto out_done;
	}

	if (MEGASAS_IS_LOGICAL(scmd) &&
	    (scmd->cmnd[0] == SYNCHRONIZE_CACHE) &&
	    (megasas_sync_cache_behavior(scmd) == DONOT_SEND_TO_FW)) {		
		/*
		 * FW takes care of flush cache on its own
		 * No need to send it down
		 */
		scmd->result = DID_OK << 16;
		goto out_done;
	}

	return instance->instancet->build_and_issue_cmd(instance,scmd);
	

 out_done:
		scmd->scsi_done(scmd);
	return 0;
}

static struct megasas_instance *megasas_lookup_instance(u16 host_no)
{
	int i;

	for (i = 0; i < megasas_mgmt_info.max_index; i++) {

		if ((megasas_mgmt_info.instance[i]) &&
		    (megasas_mgmt_info.instance[i]->host->host_no == host_no))
			return megasas_mgmt_info.instance[i];
	}

	return NULL;
}

/*
* megasas_update_sdev_properties - Update sdev structure based on controller's FW capabilities
*
* @sdev: OS provided scsi device
*
* Returns void
*/
void megasas_update_sdev_properties(struct scsi_device *sdev)
{
	u16 pd_index = 0, ld;
	u32 device_id;
	struct megasas_instance *instance;
	struct fusion_context *fusion;
	struct MR_PRIV_DEVICE *mr_device_priv_data;
	struct MR_PD_CFG_SEQ_NUM_SYNC *pd_sync;
	MR_LD_RAID *raid;
	MR_DRV_RAID_MAP_ALL *local_map_ptr;
	
	instance = megasas_lookup_instance(sdev->host->host_no);
	fusion = instance->ctrl_context;
	mr_device_priv_data = sdev->hostdata;
	
	if (!fusion || !mr_device_priv_data)
		return;

	if (sdev->channel >= MEGASAS_MAX_PD_CHANNELS) {
		device_id = ((sdev->channel % 2) * MEGASAS_MAX_DEV_PER_CHANNEL)
					+ sdev->id;
		local_map_ptr = fusion->ld_drv_map[(instance->map_id & 1)];
		ld = MR_TargetIdToLdGet(device_id, local_map_ptr);
		raid = MR_LdRaidGet(ld, local_map_ptr);

		if (raid->capability.ldPiMode == MR_PROT_INFO_TYPE_CONTROLLER)
       		blk_queue_update_dma_alignment(sdev->request_queue, 0x7);

		mr_device_priv_data->is_tm_capable = 
			raid->capability.tmCapable;
	} else if (instance->use_seqnum_jbod_fp) {
		pd_index = (sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) + 
			sdev->id;
		pd_sync = (void *)fusion->pd_seq_sync
				[(instance->pd_seq_map_id - 1) & 1];
		mr_device_priv_data->is_tm_capable =
			pd_sync->seq[pd_index].capability.tmCapable;
	}
}

inline void megasas_update_device_queue_depth(struct scsi_device *sdev,
		u32 queue_depth)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev), queue_depth);
#else
	scsi_change_queue_depth(sdev, queue_depth);
#endif
}

static void megasas_set_device_queue_depth(struct scsi_device *sdev)
{
	u16		pd_index = 0;
	int		ret = DCMD_FAILED;
	struct megasas_instance *instance;
	instance = megasas_lookup_instance(sdev->host->host_no);

	if (sdev->channel < MEGASAS_MAX_PD_CHANNELS) {
		pd_index = (sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) + sdev->id;

		if (instance->pd_info) {
			mutex_lock(&instance->hba_mutex);
			ret = megasas_get_pd_info(instance, pd_index);
			mutex_unlock(&instance->hba_mutex);
		}

		if (ret != DCMD_SUCCESS)
			return;

		if (instance->pd_list[pd_index].driveState == MR_PD_STATE_SYSTEM) {

			switch (instance->pd_list[pd_index].interface) {
			case SAS_PD:
				megasas_update_device_queue_depth(sdev, MEGASAS_SAS_QD);
				break;

			case SATA_PD:
				megasas_update_device_queue_depth(sdev, MEGASAS_SATA_QD);
				break;

			default:
				megasas_update_device_queue_depth(sdev, MEGASAS_DEFAULT_PD_QD);
			}
		}
	}
}

static int megasas_slave_configure(struct scsi_device *sdev)
{
	u16             pd_index = 0;
	struct  megasas_instance *instance ;
	
	instance = megasas_lookup_instance(sdev->host->host_no);
	if (instance->pd_list_not_supported) {
		if (sdev->channel < MEGASAS_MAX_PD_CHANNELS &&
			sdev->type == TYPE_DISK) {
			pd_index = (sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +
				sdev->id;
			if (instance->pd_list[pd_index].driveState !=
				MR_PD_STATE_SYSTEM)
				return -ENXIO;
		}
	}

	megasas_set_device_queue_depth(sdev);

	/*
	* The RAID firmware may require extended timeouts.
	*/
	blk_queue_rq_timeout(sdev->request_queue,
		scmd_timeout * HZ);

	sdev_printk(KERN_INFO, sdev, "qdepth(%d), tagged(%d), "
		"scsi_level(%d), cmd_que(%d)\n", sdev->queue_depth,
		sdev->tagged_supported, sdev->scsi_level,
		(sdev->inquiry[7] & 2) >> 1);

	megasas_update_sdev_properties(sdev);

	return 0;
}

/*
* megasas_complete_outstanding_ioctls - Complete outstanding ioctls after a
*                                       kill adapter
* @instance:		Adapter soft state
*
* Returns void
*/
void megasas_complete_outstanding_ioctls(struct megasas_instance *instance)
{
	int i;
	struct megasas_cmd *cmd_mfi;
	struct megasas_cmd_fusion *cmd_fusion;
	struct fusion_context *fusion = instance->ctrl_context;

	/* Find all outstanding ioctls */
	if (fusion) {
		for (i = 0; i < instance->max_fw_cmds; i++) {
			cmd_fusion = fusion->cmd_list[i];
			if (cmd_fusion->sync_cmd_idx != (u32)ULONG_MAX) {
				cmd_mfi = instance->cmd_list[cmd_fusion->sync_cmd_idx];
				if (cmd_mfi->sync_cmd &&
					cmd_mfi->frame->hdr.cmd != MFI_CMD_ABORT)
					megasas_complete_cmd(instance,
							     cmd_mfi, DID_OK);
			}
		}
	} else {
		for (i = 0; i < instance->max_fw_cmds; i++) {
			cmd_mfi = instance->cmd_list[i];
			if (cmd_mfi->sync_cmd && cmd_mfi->frame->hdr.cmd !=
				MFI_CMD_ABORT)
				megasas_complete_cmd(instance, cmd_mfi, DID_OK);
		}
	}
}


void megaraid_sas_kill_hba(struct megasas_instance *instance)
{
	dev_warn(&instance->pdev->dev,
		"Kill HBA from %s %d\n", __func__, __LINE__);

	/* Set critical error to block I/O & ioctls in case caller didn't */
	atomic_set(&instance->adprecovery, MEGASAS_HW_CRITICAL_ERROR);
	/* Wait 1 second to ensure IO or ioctls in build have posted */
	msleep(1000);
	if ((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0073SKINNY)) ||
		(MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0071SKINNY)) ||
		(instance->ctrl_context)) {
		if(!instance->requestorId) {
			writel(MFI_STOP_ADP,
				&instance->reg_set->doorbell);
		/* Flush */
			readl(&instance->reg_set->doorbell);
		}
		if (instance->requestorId && instance->peerIsPresent)
			memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);
	} else {
		writel(MFI_STOP_ADP,
			&instance->reg_set->inbound_doorbell);
	}
	/* Complete outstanding ioctls when adapter is killed */
	megasas_complete_outstanding_ioctls(instance);
}


void xor_gen_15x1(u32 *buff_ptrs[16], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12, *s13, *s14, *s15;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];
    s13 = buff_ptrs[13];
    s14 = buff_ptrs[14];
    s15 = buff_ptrs[15];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off] ^ s13[off] ^ s14[off] ^ s15[off];
}

void xor_gen_14x1(u32 *buff_ptrs[15], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12, *s13, *s14;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];
    s13 = buff_ptrs[13];
    s14 = buff_ptrs[14];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off] ^ s13[off] ^ s14[off];
}

void xor_gen_13x1(u32 *buff_ptrs[14], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12, *s13;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];
    s13 = buff_ptrs[13];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off] ^ s13[off];
}

void xor_gen_12x1(u32 *buff_ptrs[13], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off];
}

void xor_gen_11x1(u32 *buff_ptrs[12], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off];
}

void xor_gen_10x1(u32 *buff_ptrs[11], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off];
}

void xor_gen_9x1(u32 *buff_ptrs[10], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off];
}

void xor_gen_8x1(u32 *buff_ptrs[9], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^ s8[off];
}

void xor_gen_7x1(u32 *buff_ptrs[8], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off];
}

void xor_gen_6x1(u32 *buff_ptrs[7], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5, *s6;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off];
}

void xor_gen_5x1(u32 *buff_ptrs[6], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4, *s5;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off];
}

void xor_gen_4x1(u32 *buff_ptrs[5], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3, *s4;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off] ^ s4[off];
}

void xor_gen_3x1(u32 *buff_ptrs[4], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2, *s3;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off] ^ s3[off];
}

void xor_gen_2x1(u32 *buff_ptrs[3], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1, *s2;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off] ^ s2[off];
}

void xor_gen_1x1(u32 *buff_ptrs[2], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];

    for (words = bytes/4, off=0; words>0; words--, off++)
        d[off] = s1[off];
}

u8 xor_chk_15x1(u32 *buff_ptrs[16], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12, *s13, *s14, *s15;
    u8     xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];
    s13 = buff_ptrs[13];
    s14 = buff_ptrs[14];
    s15 = buff_ptrs[15];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off] ^ s13[off] ^ s14[off] ^ s15[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_14x1(u32 *buff_ptrs[15], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12, *s13, *s14;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];
    s13 = buff_ptrs[13];
    s14 = buff_ptrs[14];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off] ^ s13[off] ^ s14[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_13x1(u32 *buff_ptrs[14], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12, *s13;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];
    s13 = buff_ptrs[13];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off] ^ s13[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_12x1(u32 *buff_ptrs[13], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11, *s12;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];
    s12 = buff_ptrs[12];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off] ^ s12[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_11x1(u32 *buff_ptrs[12], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10, *s11;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];
    s11 = buff_ptrs[11];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off] ^ s11[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_10x1(u32 *buff_ptrs[11], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *s10;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];
    s10 = buff_ptrs[10];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off] ^ s10[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_9x1(u32 *buff_ptrs[10], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];
    s9  = buff_ptrs[9];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^\
            s8[off] ^ s9[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_8x1(u32 *buff_ptrs[9], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];
    s8  = buff_ptrs[8];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off] ^ s8[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_7x1(u32 *buff_ptrs[8], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6, *s7;
    u8    xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];
    s7  = buff_ptrs[7];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off] ^ s7[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_6x1(u32 *buff_ptrs[7], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5, *s6;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];
    s6  = buff_ptrs[6];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off] ^ s6[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_5x1(u32 *buff_ptrs[6], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4, *s5;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];
    s5  = buff_ptrs[5];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off] ^ s5[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_4x1(u32 *buff_ptrs[5], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3, *s4;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];
    s4  = buff_ptrs[4];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off] ^ s4[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_3x1(u32 *buff_ptrs[4], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2, *s3;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];
    s3  = buff_ptrs[3];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off] ^ s3[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_2x1(u32 *buff_ptrs[3], u32 bytes)
{
    u32    off, words;
    u32    r, *d, *s1, *s2;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];
    s2  = buff_ptrs[2];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        r = d[off] ^ s1[off] ^ s2[off];
        if (r) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] ^= r;
        }
    }
    return xor_result;
}

u8 xor_chk_1x1(u32 *buff_ptrs[2], u32 bytes)
{
    u32    off, words;
    u32    *d, *s1;
    u8   xor_result = MR_CPX_STATUS_SUCCESS;

    d   = buff_ptrs[0];
    s1  = buff_ptrs[1];

    for (words = bytes/4, off=0; words>0; words--, off++) {
        if (d[off] != s1[off]) {
            xor_result = MR_CPX_STATUS_INCONSISTENT;
            d[off] = s1[off];
        }
    }
    return xor_result;
}

XOR_LOW_LEVEL_GEN_FUNC  xor_gen_funcs[MAX_MR_ROW_SIZE] = {
    xor_gen_1x1,
    xor_gen_2x1,
    xor_gen_3x1,
    xor_gen_4x1,
    xor_gen_5x1,
    xor_gen_6x1,
    xor_gen_7x1,
    xor_gen_8x1,
    xor_gen_9x1,
    xor_gen_10x1,
    xor_gen_11x1,
    xor_gen_12x1,
    xor_gen_13x1,
    xor_gen_14x1,
    xor_gen_15x1,
 };

XOR_LOW_LEVEL_CHECK_FUNC  xor_check_funcs[MAX_MR_ROW_SIZE] = {
    xor_chk_1x1,
    xor_chk_2x1,
    xor_chk_3x1,
    xor_chk_4x1,
    xor_chk_5x1,
    xor_chk_6x1,
    xor_chk_7x1,
    xor_chk_8x1,
    xor_chk_9x1,
    xor_chk_10x1,
    xor_chk_11x1,
    xor_chk_12x1,
    xor_chk_13x1,
    xor_chk_14x1,
    xor_chk_15x1,
};

inline static u8 megasas_scan_set_bit(u32 bitmap)
{
    u8  bit = 0;

    while (bitmap) {
        if (bitmap & 1)
            return bit;

        bitmap >>= 1;
        bit++;
    }
    return ~0;
}

/**
 * megasas_do_cpx_xor - completes the xor operation 
 * @xor_des		: soruce and dest buffers details.
 * @host_mem		: previously mapped memory for fw
 * @host_mem_len	: mapped memory length in bytes.	
 *	
 * @return 0 on success != 0 on failure
 * 
 */
static u8 megasas_do_cpx_xor( struct mr_cpx_xor_descriptor *xor_des, const u8 *host_mem, const  u32 host_mem_len)
{

	u32 buff_valid_bit_map = xor_des->buff_valid_bitmap;
	u32 *buf_ptr_list[MAX_MR_ROW_SIZE];
	u32 tx_count = xor_des->size;
	u8 dest_idx, buf_idx, bit;
	u8          is_op_gen;    // means is XOR generation (TRUE) or check (FALSE)
	u8  status = MR_CPX_STATUS_SUCCESS;
	

	//make the first buffer ptr as the destination.
	if( xor_des->op ==  MR_CPX_XOR_OP_GEN_P || xor_des->op == MR_CPX_XOR_OP_CHECK_P )
		dest_idx = xor_des->p_idx;
	else 
		dest_idx = xor_des->q_idx;

	buf_ptr_list[0] = (u32 *)(host_mem + xor_des->buff_list[dest_idx]);
	
	is_op_gen = MR_CPX_XOR_OP_IS_GEN(xor_des->op);

	if ( xor_des->buff_list[dest_idx]+tx_count > host_mem_len){
		printk("Error: 1st host memory over flow detected.\n");
		return MR_CPX_STATUS_FAILURE;
	}

	/*
	 * For XOR_OP_CHECK_P, our check routine expects bufPtrs[0] to be both parity
	 * source and parity destination; clear out the P-index from our working bitmap
	 * so that the source-buffer scan loop below doesn't include P as one of the
	 * explicit source buffers in bufPtrs[].
	 */
	if ( !is_op_gen)
		buff_valid_bit_map &= ~(1<<xor_des->p_idx);
	
	//populate buf_ptr_list with valid buffer pointers.
	for ( buf_idx =1 ; buff_valid_bit_map != 0 ; buf_idx++ ){
		bit = megasas_scan_set_bit( buff_valid_bit_map);
		buf_ptr_list[buf_idx] = (u32 *)(host_mem + (u32)xor_des->buff_list[bit]);
		if ( xor_des->buff_list[bit]+tx_count > host_mem_len) {
			printk("Error: host memory over flow detected.\n");
			return MR_CPX_STATUS_FAILURE;;
		}
		buff_valid_bit_map &= ~(1 <<bit);
	}
	//call the xor gen fuctions.
	if ( is_op_gen )
	(*xor_gen_funcs[buf_idx-2])(buf_ptr_list, tx_count);
	else
		status = (*xor_check_funcs[buf_idx-2])(buf_ptr_list, tx_count);
	
	return status;
}

static u8 megasas_copy( struct page *page, u32 page_offset, u32 sge_offset, u8 *host_ptr, u32 len, u8 dir)
{
	u8 *page_addr;
	u32 off;
	u32 bytes_copied;
	u32 remaining;

	remaining = len;
	off = page_offset+sge_offset;

	//kmap_atomic maps single page size but os sg element can have size 
	//more than page size, handle it.
	while( remaining > 0 ){

		bytes_copied = min((size_t)remaining, (size_t)(PAGE_SIZE - (off & ~PAGE_MASK)));

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,10))
		page_addr = kmap_atomic(page+ (off >> PAGE_SHIFT), KM_SOFTIRQ0);
#else
		page_addr = kmap_atomic(page+ (off >> PAGE_SHIFT));
#endif
		if ( page_addr == NULL ){
			printk("kmap_atomic is failed.\n");
			return MR_CPX_STATUS_FAILURE;;
		}
		if ( dir == MR_CPX_DIR_WRITE )
			memcpy( host_ptr, page_addr+(off & ~PAGE_MASK), bytes_copied );
		else 
			memcpy( page_addr+(off & ~PAGE_MASK), host_ptr, bytes_copied );
	
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,10))
		kunmap_atomic ( page_addr, KM_SOFTIRQ0 );
#else
		kunmap_atomic ( page_addr);
#endif

		host_ptr += bytes_copied;
		remaining -= bytes_copied;
		off += bytes_copied;
	}
	
	return MR_CPX_STATUS_SUCCESS;
}

/**
 * megasas_do_cpx_copy - Completes the copy opreation 
 * @instance		: Driver soft state.
 * @cpy_des		: input for copying the data.
 *	
 * @return 0 on success != 0 on failure
 * 
 */
static u8 megasas_do_cpx_copy( struct megasas_instance *instance, struct mr_cpx_copy_descriptor *cpy_des)
{
	u8			status = MR_CPX_STATUS_SUCCESS ;
	u32			total_remaining_len = cpy_des->total_byte_count;
	u32			row_remaining_length;
	u32			os_sge_sz = 0, os_sge_offset = 0, os_sge_idx =0, os_sge_len;
	u8			sge_cnt, row_idx, *fw_ptr=NULL,host_skip_count_handled;
	struct scsi_cmnd	*os_cmd;
	struct scatterlist	*os_sgl;
	struct megasas_cmd	*mega_cmd;
	struct megasas_io_frame *ldio;
	
	if ( cpy_des->mfi_cmd_cxt  >= instance->max_fw_cmds ){
		printk("megasas: invalid context - 0x%x shoul be < 0x%x \n", cpy_des->mfi_cmd_cxt ,instance->max_fw_cmds );
                return MR_CPX_STATUS_FAILURE;
	}
	
	mega_cmd = ( struct megasas_cmd *)  instance->cmd_list[cpy_des->mfi_cmd_cxt];
	os_cmd = mega_cmd->scmd;
	ldio = (struct megasas_io_frame *)mega_cmd->frame;
	sge_cnt = ldio->sge_count;
	
	host_skip_count_handled = 0;
	row_idx = 0;
	row_remaining_length =0;
	
	scsi_for_each_sg(os_cmd, os_sgl, sge_cnt, os_sge_idx){
		
		os_sge_len = sg_dma_len(os_sgl);

		if ( !host_skip_count_handled  && cpy_des->host_skip_count < ( os_sge_sz += os_sge_len )  ){
			os_sge_offset = cpy_des->host_skip_count - ( os_sge_sz -os_sge_len );
			os_sge_len -= os_sge_offset;
			host_skip_count_handled = 1;
		} else if ( !host_skip_count_handled && cpy_des->host_skip_count == os_sge_sz ){
			os_sge_offset = 0;
			host_skip_count_handled = 1;
			continue;
		}
		
		if ( !host_skip_count_handled )
			continue;
		
		for( ;total_remaining_len &&  row_idx < MAX_MR_ROW_SIZE ; row_idx++ ){

			if ( ! row_remaining_length ){
				fw_ptr = (u8 *)(instance->host_mem_virt+cpy_des->copy_buf[row_idx].buf);
				row_remaining_length = cpy_des->copy_buf[row_idx].size;
			}

			if ( (status = megasas_copy( sg_page(os_sgl), os_sgl->offset, os_sge_offset, fw_ptr,
						     MIN(os_sge_len, row_remaining_length), cpy_des->dir) ) )
			     break;
			
			total_remaining_len -= MIN(os_sge_len, row_remaining_length);
			
			if ( os_sge_len <= row_remaining_length ){
				fw_ptr += os_sge_len;
				if ( !(row_remaining_length -= os_sge_len) ) row_idx++;
				os_sge_offset = 0;
				break;
			}else{
				os_sge_len -= row_remaining_length;
				os_sge_offset += row_remaining_length;
				row_remaining_length =0;
			}

		}
		if ( row_idx >= MAX_MR_ROW_SIZE && total_remaining_len )
			printk("megasas: Reached end of fw sglist while pending data transfer,row_idx = 0x%x, total_remaining_len = 0x%x \n",
			       row_idx, total_remaining_len);
		
		if( total_remaining_len == 0 || status == MR_CPX_STATUS_FAILURE )
			break;
	}

	if ( os_sge_idx >= sge_cnt && total_remaining_len )
		printk("megasas: Reached end of os sglist while pending data transfer, os_sge_idx = 0x%x, total_remaining_len = 0x%x \n",
			       os_sge_idx, total_remaining_len);
		
	return status;
	
}

/**
 * megasas_handle_cpx_requests - Manages the fw queues 
 * @instance		: Driver soft state.
 *	
 * @return 0 on success != 0 on failure
 * 
 */
int megasas_handle_cpx_requests( struct megasas_instance *instance)
{
	struct mr_cpx_request_queue *req_q = instance->cpx_request_queue;
	u32 producer_idx, consumer_idx;
	u8 retval = 0;
	unsigned long flags;

	producer_idx = req_q->producer_idx;
	consumer_idx = req_q->consumer_idx;
	
	while ((producer_idx != consumer_idx) && 
		(atomic_read(&instance->adprecovery) == MEGASAS_HBA_OPERATIONAL)){
		union  mr_cpx_descriptor *cpx_des = &instance->cpx_dscrptr[consumer_idx];
		union  mr_cpx_response_data rsp_data;

		if ( cpx_des->cpx_copy_desc.hdr.type == MR_CPX_DESCRIPTOR_TYPE_COPY )
			retval = megasas_do_cpx_copy( instance, ( struct mr_cpx_copy_descriptor *)cpx_des );
		
		else if (cpx_des->cpx_copy_desc.hdr.type == MR_CPX_DESCRIPTOR_TYPE_XOR )
			retval = megasas_do_cpx_xor( ( struct mr_cpx_xor_descriptor *)cpx_des,
						     (u8 *)instance->host_mem_virt, instance->host_mem_len );
		else{
			printk("Fatal Error : Got invalid descriptor type...\n");
			retval = MR_CPX_STATUS_FAILURE;
			break;
		}

		rsp_data.r.status = retval;
		rsp_data.r.context = cpx_des->cpx_copy_desc.hdr.context;
		rsp_data.r.type = cpx_des->cpx_copy_desc.hdr.type;

		//notify fw.
		spin_lock_irqsave(&instance->hba_lock, flags);
		writel( ~0, &instance->reg_set->inbound_high_queue_port);
		writel( rsp_data.w, &instance->reg_set->inbound_low_queue_port);
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		//take care of wrap around case.
 		consumer_idx++;
		if ( consumer_idx == instance->cpx_dscrptr_cnt )
			consumer_idx = 0;
		
	}
		req_q->consumer_idx = producer_idx;
	
	return 0;
}

 /**
  * megasas_check_and_restore_queue_depth - Check if queue depth needs to be 
  *					restored to max value
  * @instance:			Adapter soft state
  *
  */
void
megasas_check_and_restore_queue_depth(struct megasas_instance *instance)
{
	unsigned long flags;
	
	if (instance->flag & MEGASAS_FW_BUSY
	    && time_after(jiffies, instance->last_time + 5 * HZ)
	    && atomic_read(&instance->fw_outstanding) <
	    instance->throttlequeuedepth + 1) {

		spin_lock_irqsave(instance->host->host_lock, flags);
		instance->flag &= ~MEGASAS_FW_BUSY;
		instance->host->can_queue = instance->cur_can_queue;
		
		spin_unlock_irqrestore(instance->host->host_lock, flags);
	}
}

/**
 * megasas_complete_cmd_dpc	 -	Returns FW's controller structure
 * @instance_addr:			Address of adapter soft state
 *
 * Tasklet to complete cmds
 */
static void megasas_complete_cmd_dpc(unsigned long instance_addr)
{
	u32 producer;
	u32 consumer;
	u32 context;
	struct megasas_cmd *cmd;
	struct megasas_instance *instance =
				(struct megasas_instance *)instance_addr;
	unsigned long flags;

	/* If we have already declared adapter dead, donot complete cmds */
	if (atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR )
		return;

	spin_lock_irqsave(&instance->completion_lock, flags);

	producer = le32_to_cpu(*instance->producer);
	consumer = le32_to_cpu(*instance->consumer);

	while (consumer != producer) {
		context = le32_to_cpu(instance->reply_queue[consumer]);
		if (context >= instance->max_fw_cmds) {
			printk("ERROR ERROR: unexpected context value %x\n", context);
			BUG();
		}

		cmd = instance->cmd_list[context];

		megasas_complete_cmd(instance, cmd, DID_OK);

		consumer++;
		if (consumer == (instance->max_fw_cmds + 1)) {
			consumer = 0;
		}
	}

	*instance->consumer = cpu_to_le32(producer);

	spin_unlock_irqrestore(&instance->completion_lock, flags);

        if ( instance->cpx_supported )
                megasas_handle_cpx_requests( instance);

	/*
	 * Check if we can restore can_queue
	 */
	megasas_check_and_restore_queue_depth(instance);
}

/**
 * megasas_start_timer - Initializes a timer object
 * @instance:           Adapter soft state
 * @timer:              timer object to be initialized
 * @fn:                 timer function
 * @interval:           time interval between timer function call
 *
 */
void megasas_start_timer(struct megasas_instance *instance,
			 struct timer_list *timer,
			 void *fn, unsigned long interval)
{
	init_timer(timer);
	timer->expires = jiffies + interval;
	timer->data = (unsigned long)instance;
	timer->function = fn;
	add_timer(timer);
}

static void megasas_internal_reset_defer_cmds(struct megasas_instance *instance);
static void process_fw_state_change_wq(struct work_struct *work);

void megasas_do_ocr(struct megasas_instance *instance)
{
	if ((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1064R)) ||
		(MR_PCI_ID(instance->pdev) == MR_PCI_ID_DELL(PCI_DEVICE_ID_DELL_PERC5)) ||
		(MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_VERDE_ZCR)))
	{
		*instance->consumer = cpu_to_le32(MEGASAS_ADPRESET_INPROG_SIGN);
	}

	instance->instancet->disable_intr(instance);
		atomic_set(&instance->adprecovery, MEGASAS_ADPRESET_SM_INFAULT);
	instance->issuepend_done = 0;

	atomic_set(&instance->fw_outstanding, 0);
	megasas_internal_reset_defer_cmds(instance);
	process_fw_state_change_wq(&instance->work_init);
}

/* This function will get the current SR-IOV LD/VF affiliation */
static int megasas_get_ld_vf_affiliation(struct megasas_instance *instance,
	int initial)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_LD_VF_AFFILIATION *new_affiliation = NULL;
	struct MR_LD_VF_AFFILIATION_111 *new_affiliation_111 = NULL;
	struct MR_LD_VF_MAP *newmap = NULL, *savedmap = NULL;
	dma_addr_t new_affiliation_h;
	dma_addr_t new_affiliation_111_h;
	int ld, i, j, retval = 0, found = 0, doscan = 0;
	u8 thisVf;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas: megasas_get_ld_vf_"
		       "affiliation: Failed to get cmd for scsi%d.\n",
			instance->host->host_no);
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	if (!instance->vf_affiliation && !instance->vf_affiliation_111) {
		printk(KERN_WARNING "megasas: SR-IOV: Couldn't get LD/VF "
		       "affiliation for scsi%d.\n", instance->host->host_no);
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	if (initial)
		if (instance->PlasmaFW111)
			memset(instance->vf_affiliation_111, 0,
			       sizeof(struct MR_LD_VF_AFFILIATION_111));
		else
			memset(instance->vf_affiliation, 0,
			       (MAX_LOGICAL_DRIVES + 1) *
			       sizeof(struct MR_LD_VF_AFFILIATION));
	else {
		if (instance->PlasmaFW111)
			new_affiliation_111 =
				pci_alloc_consistent(instance->pdev,
						     sizeof(struct MR_LD_VF_AFFILIATION_111),
						     &new_affiliation_111_h);
		else
			new_affiliation =
				pci_alloc_consistent(instance->pdev,
						     (MAX_LOGICAL_DRIVES + 1) *
						     sizeof(struct MR_LD_VF_AFFILIATION),
						     &new_affiliation_h);
		if (!new_affiliation && !new_affiliation_111) {
			printk(KERN_DEBUG "megasas: SR-IOV: Couldn't allocate "
			       "memory for new affiliation for scsi%d.\n",
				instance->host->host_no);
			megasas_return_cmd(instance, cmd);
			return -ENOMEM;
		}
		if (instance->PlasmaFW111)
			memset(new_affiliation_111, 0,
			       sizeof(struct MR_LD_VF_AFFILIATION_111));
		else
			memset(new_affiliation, 0, (MAX_LOGICAL_DRIVES + 1) *
			       sizeof(struct MR_LD_VF_AFFILIATION));
	}

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = MFI_STAT_INVALID_STATUS;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_BOTH;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	if (instance->PlasmaFW111) {
		dcmd->data_xfer_len = sizeof(struct MR_LD_VF_AFFILIATION_111);
		dcmd->opcode = MR_DCMD_LD_VF_MAP_GET_ALL_LDS_111;
	} else {
		dcmd->data_xfer_len = (MAX_LOGICAL_DRIVES + 1) *
			sizeof(struct MR_LD_VF_AFFILIATION);
		dcmd->opcode = MR_DCMD_LD_VF_MAP_GET_ALL_LDS;
	}

	if (initial) {
		if (instance->PlasmaFW111)
			dcmd->sgl.sge32[0].phys_addr =
			  instance->vf_affiliation_111_h;
		else
			dcmd->sgl.sge32[0].phys_addr =
			  instance->vf_affiliation_h;
	} else {
		if (instance->PlasmaFW111)
			dcmd->sgl.sge32[0].phys_addr = new_affiliation_111_h;
		else
			dcmd->sgl.sge32[0].phys_addr = new_affiliation_h;
	}
	if (instance->PlasmaFW111)
		dcmd->sgl.sge32[0].length =
		  sizeof(struct MR_LD_VF_AFFILIATION_111);
	else
		dcmd->sgl.sge32[0].length = (MAX_LOGICAL_DRIVES + 1) *
			sizeof(struct MR_LD_VF_AFFILIATION);

	printk(KERN_WARNING "megasas: SR-IOV: Getting LD/VF affiliation for "
	       "scsi%d\n", instance->host->host_no);

	if (megasas_issue_blocked_cmd(instance, cmd, 0) != DCMD_SUCCESS) {
		printk(KERN_WARNING "megasas: SR-IOV: LD/VF affiliation DCMD"
		       " failed with status 0x%x for scsi%d.\n",
		       dcmd->cmd_status, instance->host->host_no);
		retval = 1; /* Do a scan if we couldn't get affiliation */
		goto out;
	}

	if (!initial) {
		if (instance->PlasmaFW111) {
			thisVf = new_affiliation_111->thisVf;
			for (ld = 0 ; ld < new_affiliation_111->vdCount; ld++)
				if (instance->vf_affiliation_111->map[ld].policy[thisVf] != new_affiliation_111->map[ld].policy[thisVf]) {
					printk(KERN_WARNING "megasas: SR-IOV: "
					       "Got new LD/VF affiliation "
					       "for scsi%d.\n",
						instance->host->host_no);
					memcpy(instance->vf_affiliation_111,
					       new_affiliation_111,
					       sizeof(struct MR_LD_VF_AFFILIATION_111));
					retval = 1;
					goto out;
				}
		} else {
			if (!new_affiliation->ldCount) {
				printk(KERN_WARNING "megasas: SR-IOV: Got new "
				       "LD/VF affiliation for passive "
				       "path for scsi%d.\n",
				       instance->host->host_no);
				retval = 1;
				goto out;
			}
			newmap = new_affiliation->map;
			savedmap = instance->vf_affiliation->map;
			thisVf = new_affiliation->thisVf;
			for (i = 0 ; i < new_affiliation->ldCount; i++) {
				found = 0;
				for (j = 0 ; j < instance->vf_affiliation->ldCount; j++) {
					if (newmap->ref.targetId == savedmap->ref.targetId) {
						found = 1;
						if (newmap->policy[thisVf] != savedmap->policy[thisVf]) {
							doscan = 1;
							goto out;
						}
					}
					savedmap = (struct MR_LD_VF_MAP *)((unsigned char *)savedmap + savedmap->size);
				}
				if (!found && newmap->policy[thisVf] != MR_LD_ACCESS_HIDDEN) {
					doscan = 1;
					goto out;
				}
				newmap = (struct MR_LD_VF_MAP *)((unsigned char *)newmap + newmap->size);
			}

			newmap = new_affiliation->map;
			savedmap = instance->vf_affiliation->map;

			for (i = 0 ; i < instance->vf_affiliation->ldCount; i++) {
				found = 0;
				for (j = 0 ; j < new_affiliation->ldCount; j++) {
					if (savedmap->ref.targetId == newmap->ref.targetId) {
						found = 1;
						if (savedmap->policy[thisVf] != newmap->policy[thisVf]) {
							doscan = 1;
							goto out;
						}
					}
					newmap = (struct MR_LD_VF_MAP *)((unsigned char *)newmap + newmap->size);
				}
	     			if (!found && savedmap->policy[thisVf] != MR_LD_ACCESS_HIDDEN) {
					doscan = 1;
					goto out;
				}
				savedmap = (struct MR_LD_VF_MAP *)((unsigned char *)savedmap + savedmap->size);
			}
		}
	}
out:
	if (doscan) {
		printk(KERN_WARNING "megasas: SR-IOV: "
		       "Got new LD/VF affiliation "
		       "for scsi%d.\n",
		       instance->host->host_no);
		memcpy(instance->vf_affiliation,
		       new_affiliation,
		       new_affiliation->size);
		retval = 1;
	}

	if (new_affiliation) {
		if (instance->PlasmaFW111)
			pci_free_consistent(instance->pdev,
					    sizeof(struct MR_LD_VF_AFFILIATION_111),
					    new_affiliation_111,
					    new_affiliation_111_h);
		else
			pci_free_consistent(instance->pdev,
					    (MAX_LOGICAL_DRIVES + 1) *
					    sizeof(struct MR_LD_VF_AFFILIATION),
					    new_affiliation, new_affiliation_h);
	}
	megasas_return_cmd(instance, cmd);

	return retval;
}

/* This function will tell FW to start the SR-IOV heartbeat */
int megasas_sriov_start_heartbeat(struct megasas_instance *instance,
					 int initial)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	int retval = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas: megasas_sriov_start_heartbeat: "
		       "Failed to get cmd for scsi%d.\n",
		       instance->host->host_no);
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	if (initial) {
		instance->hb_host_mem =
			pci_alloc_consistent(instance->pdev,
					     sizeof(struct MR_CTRL_HB_HOST_MEM),
					     &instance->hb_host_mem_h);
		if (!instance->hb_host_mem) {
			printk(KERN_DEBUG "megasas: SR-IOV: Couldn't allocate"
			       " memory for heartbeat host memory for "
			       "scsi%d.\n", instance->host->host_no);
			retval = -ENOMEM;
			goto out;
		}
		memset(instance->hb_host_mem, 0,
		       sizeof(struct MR_CTRL_HB_HOST_MEM));
	}

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->mbox.s[0] = sizeof(struct MR_CTRL_HB_HOST_MEM);
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = MFI_STAT_INVALID_STATUS;
	dcmd->sge_count = 1;
	dcmd->flags = MFI_FRAME_DIR_BOTH;
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = sizeof(struct MR_CTRL_HB_HOST_MEM);
	dcmd->opcode = MR_DCMD_CTRL_SHARED_HOST_MEM_ALLOC;
	dcmd->sgl.sge32[0].phys_addr = instance->hb_host_mem_h;
	dcmd->sgl.sge32[0].length = sizeof(struct MR_CTRL_HB_HOST_MEM);

	printk(KERN_WARNING "megasas: SR-IOV: Starting heartbeat for scsi%d\n",
	       instance->host->host_no);

	if (instance->ctrl_context && !instance->mask_interrupts)
		retval = megasas_issue_blocked_cmd(instance, cmd,
			MEGASAS_ROUTINE_WAIT_TIME_VF);
	else
		retval = megasas_issue_polled(instance, cmd);

	if (retval)
		printk(KERN_WARNING "megasas: SR-IOV: MR_DCMD_CTRL_SHARED_HOST"
		       "_MEM_ALLOC DCMD failed with status 0x%x for scsi%d\n",
		       dcmd->cmd_status,
		       instance->host->host_no);
out:
	megasas_return_cmd(instance, cmd);

	return retval;
}

/* Handler for SR-IOV heartbeat */
void megasas_sriov_heartbeat_handler(unsigned long instance_addr)
{
	struct megasas_instance *instance =
		(struct megasas_instance *)instance_addr;

	if (instance->hb_host_mem->HB.fwCounter !=
	    instance->hb_host_mem->HB.driverCounter) {
		instance->hb_host_mem->HB.driverCounter =
			instance->hb_host_mem->HB.fwCounter;
		mod_timer(&instance->sriov_heartbeat_timer,
			  jiffies + MEGASAS_SRIOV_HEARTBEAT_INTERVAL_VF);
	} else {
		printk(KERN_WARNING "megasas: SR-IOV: Heartbeat never "
		       "completed for scsi%d\n", instance->host->host_no);
		schedule_work(&instance->work_init);
	}
}


/**
 * megasas_wait_for_outstanding -	Wait for all outstanding cmds
 * @instance:				Adapter soft state
 *
 * This function waits for upto MEGASAS_RESET_WAIT_TIME seconds for FW to
 * complete all its outstanding commands. Returns error if one or more IOs
 * are pending after this time period. It also marks the controller dead.
 */
static int megasas_wait_for_outstanding(struct megasas_instance *instance)
{
	int i, sl, outstanding;
	u32 reset_index;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;
	u8 adp_state;
	unsigned long flags;
	struct list_head clist_local;
	struct megasas_cmd *reset_cmd;
	u32 fw_state;

	// If we are in-process if internal reset, we should wait for that process to
	// complete
	adp_state = atomic_read(&instance->adprecovery);

	if (adp_state == MEGASAS_HW_CRITICAL_ERROR) {
		dev_info(&instance->pdev->dev, "%s:%d HBA is killed.\n",
		__func__, __LINE__);
		return FAILED;
	}

	if (adp_state != MEGASAS_HBA_OPERATIONAL) {

		// We take the ownership of all the pending commands. These would be failed to the OS
		// after a successful recovery from adapter internal reset condition.
		INIT_LIST_HEAD(&clist_local);
		spin_lock_irqsave(&instance->hba_lock, flags);
		list_splice_init(&instance->internal_reset_pending_q, &clist_local);
		spin_unlock_irqrestore(&instance->hba_lock, flags);

		dev_info(&instance->pdev->dev, "%s:%d HBA reset handler invoked while adapter"
			"internal reset in progress, wait till that's over...\n",
			__func__, __LINE__);
		for (i = 0; i < wait_time; i++) {
			msleep(1000);
			// Are we there yet?
			adp_state = atomic_read(&instance->adprecovery);
			if (adp_state == MEGASAS_HBA_OPERATIONAL)
				break;
		}

		// Are we out of reset yet? If not, HBA is toasted :-(
		if (adp_state != MEGASAS_HBA_OPERATIONAL) {
			dev_info(&instance->pdev->dev,
				"%s:%d HBA reset handler timedout for internal reset. Stopping the HBA \n",
				__func__, __LINE__);
			atomic_set(&instance->adprecovery, MEGASAS_HW_CRITICAL_ERROR);
			return FAILED;
		}

		dev_info(&instance->pdev->dev,
			"%s:%d HBA internal reset condition discovered to be cleared.\n",
			__func__, __LINE__);

		// Send the pending commands back to the OS with reset condition
		reset_index	= 0;
		while (!list_empty(&clist_local)) {
			reset_cmd	= list_entry((&clist_local)->next, struct megasas_cmd, list);
			list_del_init(&reset_cmd->list);
			if (reset_cmd->scmd) {
				reset_cmd->scmd->result = DID_REQUEUE << 16;
				dev_info(&instance->pdev->dev,
					"%s:%d %d:%p reset scsi command [%02x], %#lx\n",
					__func__, __LINE__, reset_index, reset_cmd, 
					reset_cmd->scmd->cmnd[0], reset_cmd->scmd->serial_number);
				reset_cmd->scmd->scsi_done(reset_cmd->scmd);
				megasas_return_cmd(instance, reset_cmd);
			}
			else if (reset_cmd->sync_cmd) {
				// Such commands have no timeout, we re-issue this guy again.
				dev_info(&instance->pdev->dev,
					"%s:%d %p synchronous command detected on the internal reset queue, re-issuing it.\n",
					__func__, __LINE__, reset_cmd);
				reset_cmd->cmd_status_drv = MFI_STAT_INVALID_STATUS;
				instance->instancet->fire_cmd(instance, reset_cmd->frame_phys_addr ,0,instance->reg_set);
			}
			else {
				dev_info(&instance->pdev->dev,
					"%s:%d %p unexpected command on the internal reset defer list.\n",
					__func__, __LINE__, reset_cmd);
			}
			reset_index++;
		}

		dev_info(&instance->pdev->dev,
			"%s:%d All pending commands have been cleared for reset condition.\n",
			__func__, __LINE__);

		return SUCCESS;
	}
	

	// Kernel reset without internal reset in progress. HBA is operational
	dev_info(&instance->pdev->dev,
		"%s:%d HBA reset handler invoked without an internal reset condition.\n",
		__func__, __LINE__);

	for (i = 0; i < resetwaittime; i++) {
		outstanding = atomic_read(&instance->fw_outstanding);

		if (!outstanding)
			break;

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			dev_info(&instance->pdev->dev,
				"%s:%d [%2d]waiting for %d commands to complete\n",
				__func__, __LINE__, i,outstanding);
			/*
			 * Call cmd completion routine. Cmd to be
			 * be completed directly without depending on isr.
			 */

			megasas_complete_cmd_dpc((unsigned long)instance);
		}
		msleep(1000);
	}

	i = 0;
	outstanding = atomic_read(&instance->fw_outstanding);
	fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) & MFI_STATE_MASK;

	if((!outstanding && (fw_state == MFI_STATE_OPERATIONAL)))
		goto no_outstanding;

	if(instance->disableOnlineCtrlReset)
		goto kill_hba_and_failed;
		
	do {
                if ((fw_state == MFI_STATE_FAULT) || atomic_read(&instance->fw_outstanding) ) {
			dev_info(&instance->pdev->dev,
				"%s:%d waiting_for_outstanding: before issue OCR. FW state = 0x%x, oustanding 0x%x\n",
				__func__, __LINE__, fw_state, atomic_read(&instance->fw_outstanding));
			if (i == 3)
				goto kill_hba_and_failed;
                        megasas_do_ocr(instance);
			adp_state = atomic_read(&instance->adprecovery);
			if (adp_state == MEGASAS_HW_CRITICAL_ERROR) {
				dev_info(&instance->pdev->dev, "%s:%d OCR failed and HBA is killed.\n",
				__func__, __LINE__);
				return FAILED;
			}
			dev_info(&instance->pdev->dev, "%s:%d waiting_for_outstanding: after issue OCR.\n",
				__func__, __LINE__);

                        /* wait for 5 secs to let the FW finish all the pending cmds*/
                        for (sl=0; sl<10; sl++)
                                msleep(500);

			outstanding = atomic_read(&instance->fw_outstanding);

                	fw_state = instance->instancet->read_fw_status_reg(instance->reg_set) & MFI_STATE_MASK;
			if((!outstanding && (fw_state == MFI_STATE_OPERATIONAL)))
				goto no_outstanding;
		}
		i++;
	} while (i <= 3);

no_outstanding:

	dev_info(&instance->pdev->dev, "%s:%d no more pending commands remain after reset handling.\n",
		__func__, __LINE__);
	return SUCCESS;

kill_hba_and_failed:

	/* Reset not supported, kill adapter */
	dev_info(&instance->pdev->dev, "%s:%d killing adapter scsi%d"
	 " disableOnlineCtrlReset %d fw_outstanding %d \n",
		__func__, __LINE__, instance->host->host_no, instance->disableOnlineCtrlReset,
		atomic_read(&instance->fw_outstanding));
	megasas_dump_pending_frames(instance);
	megaraid_sas_kill_hba(instance);

	return FAILED;

}

/**
 * megasas_generic_reset -	Generic reset routine
 * @scmd:			Mid-layer SCSI command
 *
 * This routine implements a generic reset handler for device, bus and host
 * reset requests. Device, bus and host specific reset handlers can use this
 * function after they do their specific tasks.
 */
static int megasas_generic_reset(struct scsi_cmnd *scmd)
{
	int ret_val;
	struct megasas_instance *instance;

	instance = (struct megasas_instance *)scmd->device->host->hostdata;


	scmd_printk(KERN_NOTICE, scmd, "megasas: RESET -%ld cmd=%x retries=%x\n",
		 scmd->serial_number, scmd->cmnd[0], scmd->retries);

	if (atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR) {
		printk(KERN_ERR "megasas: cannot recover from previous reset "
		       "failures\n");
		return FAILED;
	}

	ret_val = megasas_wait_for_outstanding(instance);
	if (ret_val == SUCCESS)
		printk(KERN_NOTICE "megasas: reset successful \n");
	else
		printk(KERN_ERR "megasas: failed to do reset\n");
	

	return ret_val;
}

/**
 * megasas_reset_timer - quiesce the adapter if required
 * @scmd:		scsi cmnd
 *
 * Sets the FW busy flag and reduces the host->can_queue if the
 * cmd has not been completed within the timeout period.
 */
static enum
blk_eh_timer_return megasas_reset_timer(struct scsi_cmnd *scmd)
{
	struct megasas_instance *instance;
	unsigned long flags;

	if (time_after(jiffies, scmd->jiffies_at_alloc +
				(scmd_timeout * 2) * HZ)) {
		return BLK_EH_NOT_HANDLED;
	}

	instance = (struct megasas_instance *)scmd->device->host->hostdata;
	if (!(instance->flag & MEGASAS_FW_BUSY)) {
		/* FW is busy, throttle IO */
		spin_lock_irqsave(instance->host->host_lock, flags);

		instance->host->can_queue = instance->throttlequeuedepth;
		instance->last_time = jiffies;
		instance->flag |= MEGASAS_FW_BUSY;

		spin_unlock_irqrestore(instance->host->host_lock, flags);
	}
	return BLK_EH_RESET_TIMER;
}

/**
 * megasas_reset_bus_host -	Bus & host reset handler entry point
 */
static int megasas_reset_bus_host(struct scsi_cmnd *scmd)
{
	int ret;
	struct megasas_instance *instance;
	instance = (struct megasas_instance *)scmd->device->host->hostdata;
	
	if (instance->ctrl_context)
		ret = megasas_reset_fusion(scmd->device->host, SCSIIO_TIMEOUT_OCR);
	else
		ret = megasas_generic_reset(scmd);

	return ret;
}

static int megasas_task_abort(struct scsi_cmnd *scmd)
{
	int ret;
	struct megasas_instance *instance;
	instance = (struct megasas_instance *)scmd->device->host->hostdata;
	
	if (instance->ctrl_context)
		ret = megasas_task_abort_fusion(scmd);
	else {
		sdev_printk(KERN_NOTICE, scmd->device, "TASK ABORT not supported\n");
		ret = FAILED;
	}

	return ret;

}

static int megasas_reset_target(struct scsi_cmnd *scmd)
{
	int ret;
	struct megasas_instance *instance;
	instance = (struct megasas_instance *)scmd->device->host->hostdata;
	
	if (instance->ctrl_context)
		ret = megasas_reset_target_fusion(scmd);
	else {
		sdev_printk(KERN_NOTICE, scmd->device, "TARGET RESET not supported\n");
		ret = FAILED;
	}

	return ret;

}
/**
 * megasas_bios_param - Returns disk geometry for a disk
 * @sdev: 		device handle
 * @bdev:		block device
 * @capacity:		drive capacity
 * @geom:		geometry parameters
 */
static int
megasas_bios_param(struct scsi_device *sdev, struct block_device *bdev,
		 sector_t capacity, int geom[])
{
	int heads;
	int sectors;
	sector_t cylinders;
	unsigned long tmp;
	/* Default heads (64) & sectors (32) */
	heads = 64;
	sectors = 32;

	tmp = heads * sectors;
	cylinders = capacity;

	sector_div(cylinders, tmp);

	/*
	 * Handle extended translation size for logical drives > 1Gb
	 */

	if (capacity >= 0x200000) {
		heads = 255;
		sectors = 63;
		tmp = heads*sectors;
		cylinders = capacity;
		sector_div(cylinders, tmp);
	}

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;

	return 0;
}

static void megasas_aen_polling(struct work_struct *work);

/**
 * megasas_service_aen -	Processes an event notification
 * @instance:			Adapter soft state
 * @cmd:			AEN command completed by the ISR
 *
 * For AEN, driver sends a command down to FW that is held by the FW till an
 * event occurs. When an event of interest occurs, FW completes the command
 * that it was previously holding.
 *
 * This routines sends SIGIO signal to processes that have registered with the
 * driver for AEN.
 */
static void
megasas_service_aen(struct megasas_instance *instance, struct megasas_cmd *cmd)
{
	unsigned long flags;
	/*
	 * Don't signal app if it is just an aborted previously registered aen
	 */
	if ((!cmd->abort_aen) && (instance->unload == 0)) {
		spin_lock_irqsave(&poll_aen_lock, flags);
		megasas_poll_wait_aen = 1;
		spin_unlock_irqrestore(&poll_aen_lock, flags);
		wake_up(&megasas_poll_wait);
		kill_fasync(&megasas_async_queue, SIGIO, POLL_IN);
	}
	else
		cmd->abort_aen = 0;

	instance->aen_cmd = NULL;

	megasas_return_cmd(instance, cmd);

	if ((instance->unload == 0) && ((instance->issuepend_done == 1))) {
		struct megasas_aen_event *ev;
		ev = kzalloc(sizeof(*ev), GFP_ATOMIC);
		if (!ev) {
			printk(KERN_ERR "megasas_service_aen: out of memory\n");
		} else {
			ev->instance = instance;
			instance->ev = ev;
			INIT_DELAYED_WORK(&ev->hotplug_work, megasas_aen_polling);
			schedule_delayed_work(&ev->hotplug_work, 0);
		}
	}
}

static int megasas_slave_alloc(struct scsi_device *sdev)
{
	u16             pd_index = 0;
	struct megasas_instance *instance ;
	struct MR_PRIV_DEVICE *mr_device_priv_data;
	instance = megasas_lookup_instance(sdev->host->host_no);
	if (sdev->channel < MEGASAS_MAX_PD_CHANNELS) {
		/*
		 * Open the OS scan to the SYSTEM PD
		 */
		pd_index =
			(sdev->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +
			sdev->id;
		/* 
 		 * Expose SCSI device if it is marked as system pd or
		 * driveType is other than TYPE_DISK...due to some FW does not
		 * expose TYPE_TAPE as system pd.
		 */
		if (instance->pd_list_not_supported ||
				(instance->pd_list[pd_index].driveState ==
				MR_PD_STATE_SYSTEM))
			goto scan_target;

		return -ENXIO;
	}

scan_target:
	mr_device_priv_data = kzalloc(sizeof(*mr_device_priv_data),
					GFP_KERNEL);
	if (!mr_device_priv_data)
		return -ENOMEM;
	sdev->hostdata = mr_device_priv_data;

	atomic_set(&mr_device_priv_data->r1_ldio_hint,
			instance->r1_ldio_hint_default);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
	sdev->tagged_supported = 1;
	scsi_activate_tcq(sdev, sdev->queue_depth);
#endif

	return 0;
}

static void megasas_slave_destroy(struct scsi_device *sdev)
{
	kfree(sdev->hostdata);
	sdev->hostdata = NULL;
}
#if ((defined(CONFIG_SUSE_KERNEL) || defined(RHEL_RELEASE_CODE)) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32) || \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33))
#define RAMP_UP_SUPPORT
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
#if defined(RAMP_UP_SUPPORT)
static int megasas_change_queue_depth(struct scsi_device *sdev,
				      int queue_depth, int reason)
{
	if (reason != SCSI_QDEPTH_DEFAULT)
		return -EOPNOTSUPP;

#else
static int megasas_change_queue_depth(struct scsi_device *sdev,
				      int queue_depth)
{
#endif
	if (queue_depth > sdev->host->can_queue)
		queue_depth = sdev->host->can_queue;
	scsi_adjust_queue_depth(sdev, scsi_get_tag_type(sdev),
				queue_depth);

	return queue_depth;
}
#endif

static ssize_t
megasas_fw_crash_buffer_store(struct device *cdev, struct device_attribute *attr,
        const char *buf, size_t count)
{
        struct Scsi_Host *shost = class_to_shost(cdev);
        struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;
        int val = 0;
	unsigned long flags;

        if (sscanf(buf, "%d", &val) != 1)
                return -EINVAL;

	spin_lock_irqsave(&instance->crashdump_lock, flags);
        instance->fw_crash_buffer_offset = val;
	spin_unlock_irqrestore(&instance->crashdump_lock, flags);
        return strlen(buf);
}

static ssize_t
megasas_fw_crash_buffer_show(struct device *cdev, struct device_attribute *attr,
        char *buf)
{
        struct Scsi_Host *shost = class_to_shost(cdev);
        struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;
        u32 size;
        unsigned long buff_addr;
        unsigned long dmachunk = 1024 * 1024;
        unsigned long src_addr;
	unsigned long flags;
	
	spin_lock_irqsave(&instance->crashdump_lock, flags);
	if (!instance->crash_dump_buf && !((instance->fw_crash_state == AVAILABLE) ||
		(instance->fw_crash_state == COPYING))) {
		printk("megasas: FW crash dump is not available\n");
		spin_unlock_irqrestore(&instance->crashdump_lock, flags);
		return -EINVAL;
	}
		
        buff_addr = (unsigned long) buf;

       if (instance->fw_crash_buffer_offset > (instance->fw_crash_buffer_size * 1024 *1024)) {
                printk("megasas: passed crash dump offset is out of range\n");
		spin_unlock_irqrestore(&instance->crashdump_lock, flags);
                return 0;
        }

        size = (instance->fw_crash_buffer_size * 1024 * 1024) - instance->fw_crash_buffer_offset;
        size = (size >= PAGE_SIZE) ? (PAGE_SIZE - 1) : size;

        src_addr =  (unsigned long)instance->crash_buf[instance->fw_crash_buffer_offset / dmachunk] +
                        (instance->fw_crash_buffer_offset % dmachunk);
        memcpy(buf, (void*)src_addr,  size);
	spin_unlock_irqrestore(&instance->crashdump_lock, flags);
        
        return (size);
}

static ssize_t
megasas_fw_crash_buffer_size_show (struct device *cdev, struct device_attribute *attr,
        char *buf)
{
        struct Scsi_Host *shost = class_to_shost(cdev);
        struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;

        return snprintf(buf, PAGE_SIZE, "%ld\n", (unsigned long)((instance->fw_crash_buffer_size) * 1024 * 1024)/PAGE_SIZE);
}

static ssize_t
megasas_fw_crash_state_store(struct device *cdev, struct device_attribute *attr,
        const char *buf, size_t count)
{
        struct Scsi_Host *shost = class_to_shost(cdev);
        struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;
        int val = 0;
	unsigned long flags;

        sscanf(buf, "%d", &val);
        
	if ((val <= AVAILABLE || val > COPY_ERROR )) {
                printk("megasas: application updates invalid fw crash state\n");
                return -EINVAL;
        }

        instance->fw_crash_state = val;

        if ((val == COPIED) || (val == COPY_ERROR)) {
                /* free host crash  buffers here */
		spin_lock_irqsave(&instance->crashdump_lock, flags);
               	megasas_free_host_crash_buffer(instance); 
		spin_unlock_irqrestore(&instance->crashdump_lock, flags);
                if (val == COPY_ERROR)
                        printk("megasas: application failed to copy crash dump\n");
                else
                        printk("megasas: crash dump copied successfully\n");

        }
        return strlen(buf);
}

static ssize_t
megasas_fw_crash_state_show (struct device *cdev, struct device_attribute *attr,
        char *buf)
{
        struct Scsi_Host *shost = class_to_shost(cdev);
        struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;

        return snprintf(buf, PAGE_SIZE, "%d\n", instance->fw_crash_state);
}

static ssize_t
megasas_preffered_count_store(struct device *cdev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;
	u32 val = 0;

	sscanf(buf, "%u", &val);

	if (val < 4 || val > 16)
		instance->r1_ldio_hint_default =  MR_R1_LDIO_PIGGYBACK_DEFAULT;
	else
		instance->r1_ldio_hint_default = val;

	return strlen(buf);
}

static ssize_t
megasas_preffered_count_show (struct device *cdev, struct device_attribute *attr,
	char *buf)
{
	struct Scsi_Host *shost = class_to_shost(cdev);
	struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;

	return snprintf(buf, PAGE_SIZE, "%ld\n", (unsigned long) instance->r1_ldio_hint_default);
}


static ssize_t
megasas_page_size_show(struct device *cdev, struct device_attribute *attr,
        char *buf)
{
        return snprintf(buf, PAGE_SIZE, "%ld\n", (unsigned long)PAGE_SIZE - 1);
}

static ssize_t
megasas_ldio_outstanding_show (struct device *cdev, struct device_attribute *attr,
        char *buf)
{
        struct Scsi_Host *shost = class_to_shost(cdev);
        struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;

        return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&instance->ldio_outstanding));
}

static ssize_t
megasas_fw_cmd_outstanding_show (struct device *cdev, struct device_attribute *attr,
        char *buf)
{
        struct Scsi_Host *shost = class_to_shost(cdev);
        struct megasas_instance *instance = (struct megasas_instance *)shost->hostdata;

        return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&instance->fw_outstanding));
}


static DEVICE_ATTR(fw_crash_buffer, S_IRUGO | S_IWUSR,
        megasas_fw_crash_buffer_show, megasas_fw_crash_buffer_store);
static DEVICE_ATTR(fw_crash_buffer_size, S_IRUGO,
        megasas_fw_crash_buffer_size_show, NULL);
static DEVICE_ATTR(fw_crash_state, S_IRUGO | S_IWUSR,
        megasas_fw_crash_state_show, megasas_fw_crash_state_store);
static DEVICE_ATTR(page_size, S_IRUGO,
        megasas_page_size_show, NULL);
static DEVICE_ATTR(ldio_outstanding, S_IRUGO,
        megasas_ldio_outstanding_show, NULL);
static DEVICE_ATTR(fw_cmd_outstanding, S_IRUGO,
        megasas_fw_cmd_outstanding_show, NULL);
static DEVICE_ATTR(preffered_count, S_IRUGO | S_IWUSR,
	megasas_preffered_count_show, megasas_preffered_count_store);

struct device_attribute *megaraid_host_attrs[] = {
        &dev_attr_fw_crash_buffer_size,
        &dev_attr_fw_crash_buffer,
        &dev_attr_fw_crash_state,
        &dev_attr_page_size,
        &dev_attr_ldio_outstanding,
        &dev_attr_fw_cmd_outstanding,
	&dev_attr_preffered_count,
        NULL,
};

/*
 * Scsi host template for megaraid_sas driver
 */
static struct scsi_host_template megasas_template = {

	.module = THIS_MODULE,
	.name = "Avago SAS based MegaRAID driver",
	.proc_name = "megaraid_sas",
	.slave_configure = megasas_slave_configure,
	.slave_alloc = megasas_slave_alloc,
	.slave_destroy = megasas_slave_destroy,
	.queuecommand = megasas_queue_command,
	.eh_target_reset_handler = megasas_reset_target,
	.eh_abort_handler = megasas_task_abort,
	.eh_host_reset_handler = megasas_reset_bus_host,
	.eh_timed_out = megasas_reset_timer,
	.shost_attrs = megaraid_host_attrs,
	.bios_param = megasas_bios_param,
	.use_clustering = ENABLE_CLUSTERING,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
	.change_queue_depth = megasas_change_queue_depth,
#else
	.change_queue_depth = scsi_change_queue_depth,
#endif
};

/**
 * megasas_complete_int_cmd -	Completes an internal command
 * @instance:			Adapter soft state
 * @cmd:			Command to be completed
 *
 * The megasas_issue_blocked_cmd() function waits for a command to complete
 * after it issues a command. This function wakes up that waiting routine by
 * calling wake_up() on the wait queue.
 */
static void
megasas_complete_int_cmd(struct megasas_instance *instance,
			 struct megasas_cmd *cmd)
{
	cmd->cmd_status_drv = cmd->frame->io.cmd_status;
	wake_up(&instance->int_cmd_wait_q);
}

/**
 * megasas_complete_abort -	Completes aborting a command
 * @instance:			Adapter soft state
 * @cmd:			Cmd that was issued to abort another cmd
 *
 * The megasas_issue_blocked_abort_cmd() function waits on abort_cmd_wait_q 
 * after it issues an abort on a previously issued command. This function 
 * wakes up all functions waiting on the same wait queue.
 */
static void
megasas_complete_abort(struct megasas_instance *instance,
		       struct megasas_cmd *cmd)
{
	if (cmd->sync_cmd) {
		cmd->sync_cmd = 0;
		cmd->cmd_status_drv = 0;
		wake_up(&instance->abort_cmd_wait_q);
	}

	return;
}

/**
 * megasas_complete_cmd -	Completes a command
 * @instance:			Adapter soft state
 * @cmd:			Command to be completed
 * @alt_status:			If non-zero, use this value as status to 
 * 				SCSI mid-layer instead of the value returned
 * 				by the FW. This should be used if caller wants
 * 				an alternate status (as in the case of aborted
 * 				commands)
 */
void
megasas_complete_cmd(struct megasas_instance *instance, struct megasas_cmd *cmd,
		     u8 alt_status)
{
	unsigned long flags;
	int exception = 0;
	struct megasas_header *hdr = &cmd->frame->hdr;
	struct fusion_context *fusion = instance->ctrl_context;
	u32 opcode;
	u32 status;

       // If the commands complete successfully, the retry counter should also be reset
        // for future re-tries.
        cmd->retry_for_fw_reset = 0;

	if (cmd->scmd)
		cmd->scmd->SCp.ptr = NULL;

	switch (hdr->cmd) {
	case MFI_CMD_INVALID:
		/* Some older 1068 controller FW may keep a pended
                   MR_DCMD_CTRL_EVENT_GET_INFO left over from the main kernel
                   when booting the kdump kernel.  Ignore this command to
                   prevent a kernel panic on shutdown of the kdump kernel. */
		printk(KERN_WARNING "megaraid_sas: MFI_CMD_INVALID command "
		       "completed.\n");
		printk(KERN_WARNING "megaraid_sas: If you have a controller "
		       "other than PERC5, please upgrade your firmware.\n");
		break;
	case MFI_CMD_PD_SCSI_IO:
	case MFI_CMD_LD_SCSI_IO:

		/*
		 * MFI_CMD_PD_SCSI_IO and MFI_CMD_LD_SCSI_IO could have been
		 * issued either through an IO path or an IOCTL path. If it
		 * was via IOCTL, we will send it to internal completion.
		 */
		if (cmd->sync_cmd) {
			cmd->sync_cmd = 0;
			megasas_complete_int_cmd(instance, cmd);
			break;
		}

	case MFI_CMD_LD_READ:
	case MFI_CMD_LD_WRITE:

		if (alt_status) {
			cmd->scmd->result = alt_status << 16;
			exception = 1;
		}

		if (exception) {

			atomic_dec(&instance->fw_outstanding);

			scsi_dma_unmap(cmd->scmd);
			cmd->scmd->scsi_done(cmd->scmd);
			megasas_return_cmd(instance, cmd);

			break;
		}

		switch (hdr->cmd_status) {

		case MFI_STAT_OK:
			cmd->scmd->result = DID_OK << 16;
			break;

		case MFI_STAT_SCSI_IO_FAILED:
		case MFI_STAT_LD_INIT_IN_PROGRESS:
			cmd->scmd->result =
			    (DID_ERROR << 16) | hdr->scsi_status;
			break;

		case MFI_STAT_SCSI_DONE_WITH_ERROR:

			cmd->scmd->result = (DID_OK << 16) | hdr->scsi_status;

			if (hdr->scsi_status == SAM_STAT_CHECK_CONDITION) {
				memset(cmd->scmd->sense_buffer, 0,
				       SCSI_SENSE_BUFFERSIZE);
				memcpy(cmd->scmd->sense_buffer, cmd->sense,
				       hdr->sense_len);

				cmd->scmd->result |= DRIVER_SENSE << 24;
			}

			break;

		case MFI_STAT_LD_OFFLINE:
		case MFI_STAT_DEVICE_NOT_FOUND:
			cmd->scmd->result = DID_BAD_TARGET << 16;
			break;

		default:
			printk(KERN_DEBUG "megasas: MFI FW status %#x\n",
			       hdr->cmd_status);
			cmd->scmd->result = DID_ERROR << 16;
			break;
		}

		atomic_dec(&instance->fw_outstanding);

		scsi_dma_unmap(cmd->scmd);
		cmd->scmd->scsi_done(cmd->scmd);
		megasas_return_cmd(instance, cmd);

		break;

	case MFI_CMD_SMP:
	case MFI_CMD_STP:
	case MFI_CMD_DCMD:
		opcode = le32_to_cpu(cmd->frame->dcmd.opcode);
		/* Check for LD map update */
		if ((opcode == MR_DCMD_LD_MAP_GET_INFO)
			&& (cmd->frame->dcmd.mbox.b[1] == 1)) {
	
			fusion->fast_path_io = 0;
			spin_lock_irqsave(instance->host->host_lock, flags);
			instance->map_update_cmd = NULL;
			if (cmd->frame->hdr.cmd_status != 0) {
				if (cmd->frame->hdr.cmd_status != MFI_STAT_NOT_FOUND)
					printk(KERN_WARNING "megasas: map sync failed, status = 0x%x.\n", cmd->frame->hdr.cmd_status);
				else {
					/* LD MAP is only used for Fusion controller, 
					 * so it is safe to call mfi_mpt free routine without checking controller type.
					 */
					megasas_return_cmd(instance, cmd);
					spin_unlock_irqrestore(instance->host->host_lock, flags);
					break;
				}
			} else
				instance->map_id++;
			megasas_return_cmd(instance, cmd);

			// Set fast path io to ZERO. Validate Map will set proper value. Meanwhile all IOs will go 
			// as LD IO.
			if (MR_ValidateMapInfo(instance))
				fusion->fast_path_io = 1;
			else
				fusion->fast_path_io = 0;
			megasas_sync_map_info(instance);
			spin_unlock_irqrestore(instance->host->host_lock, flags);
			break;
		}

		/* FW has an updated PD sequence */
		if ((opcode == MR_DCMD_SYSTEM_PD_MAP_GET_INFO) &&
			(cmd->frame->dcmd.mbox.b[0] == 1)) {

			spin_lock_irqsave(instance->host->host_lock, flags);
			status = cmd->frame->hdr.cmd_status;
			instance->jbod_seq_cmd = NULL;
			megasas_return_cmd(instance, cmd);

			if (status == MFI_STAT_OK) {
				instance->pd_seq_map_id++;
				/* Re-register a pd sync seq num cmd */
				if (megasas_sync_pd_seq_num(instance, true))
					instance->use_seqnum_jbod_fp = 0;
			} else {
				instance->use_seqnum_jbod_fp = 0;
				dev_err(&instance->pdev->dev,
					"Failed from %s %d\n", __func__,
					__LINE__);
			}

			spin_unlock_irqrestore(instance->host->host_lock, flags);
			break;
		}

		/*
		 * See if got an event notification
		 */
		if (opcode == MR_DCMD_CTRL_EVENT_WAIT)
			megasas_service_aen(instance, cmd);
		else
			megasas_complete_int_cmd(instance, cmd);

		break;

	case MFI_CMD_ABORT:
		/*
		 * Cmd issued to abort another cmd returned
		 */
		megasas_complete_abort(instance, cmd);
		break;

	default:
		printk("megasas: Unknown command completed! [0x%X]\n",
		       hdr->cmd);
		break;
	}
}

/**
 * megasas_issue_pending_cmds_again -	issue all pending cmds
 *                              	in FW again because of the fw reset
 * @instance:				Adapter soft state
 */
static inline void
megasas_issue_pending_cmds_again(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct list_head clist_local;
	union megasas_evt_class_locale class_locale;
	unsigned long flags;
	u32 seq_num;

	INIT_LIST_HEAD(&clist_local);
	spin_lock_irqsave(&instance->hba_lock, flags);
	list_splice_init(&instance->internal_reset_pending_q, &clist_local);
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	while (!list_empty(&clist_local)) {
		cmd	= list_entry((&clist_local)->next, struct megasas_cmd, list);
		list_del_init(&cmd->list);

               if (cmd->sync_cmd || cmd->scmd) {
                       printk("megaraid_sas: command %p, %p:%d detected to be pending while HBA reset.\n", cmd, cmd->scmd, cmd->sync_cmd);

                       cmd->retry_for_fw_reset++;

                       // If a command has continuously been tried multiple times and causing
                       // a FW reset condition, no further recoveries should be performed on
                       // the controller
                       if (cmd->retry_for_fw_reset == 3) {
                               printk("megaraid_sas: command %p, %p:%d was tried multiple times during adapter reset. Shutting down the HBA\n", cmd, cmd->scmd, cmd->sync_cmd);
				instance->instancet->disable_intr(instance);
				atomic_set(&instance->fw_reset_no_pci_access, 1);
				megaraid_sas_kill_hba(instance);
				return;
                       }
               }

		if (cmd->sync_cmd == 1) {
			if (cmd->scmd) {
				printk("megaraid_sas: unexpected SCSI command attached to internal command!\n");
			}
			printk("megasas: %p synchronous command detected on the internal reset queue, issue it again.\n", cmd);
			cmd->cmd_status_drv = MFI_STAT_INVALID_STATUS; 
			instance->instancet->fire_cmd(instance,cmd->frame_phys_addr ,0,instance->reg_set);
		} else if (cmd->scmd) {
			printk("megasas: %p scsi command [%02x], %#lx detected on the internal reset queue, issue it again.\n", cmd, cmd->scmd->cmnd[0], cmd->scmd->serial_number);
			atomic_inc(&instance->fw_outstanding);
			instance->instancet->fire_cmd(instance, cmd->frame_phys_addr ,cmd->frame_count-1,instance->reg_set);
		}
		else {
			printk("megasas: %p unexpected command on the internal reset defer list while re-issue!!\n", cmd);
		}
	}

	// Re-register AEN
	if (instance->aen_cmd) {
		printk("megaraid_sas: existing aen_cmd discovered in deferred processing, freeing...\n");
		megasas_return_cmd(instance, instance->aen_cmd);
		instance->aen_cmd	= NULL;
	}

	/*
	* Initiate AEN (Asynchronous Event Notification)
	*/
	seq_num = instance->last_seq_num;
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	megasas_register_aen(instance, seq_num, class_locale.word);


}

/**
 * Move the internal reset pending commands to a deferred queue.
 *
 * We move the commands pending at internal reset time to a pending queue. This queue would
 * be flushed after successful completion of the internal reset sequence.
 * if the internal reset did not complete in time, the kernel reset handler would flush these
 * commands.
 **/
static void megasas_internal_reset_defer_cmds(struct megasas_instance *instance)
{
        struct megasas_cmd *cmd;
        int i;
        u32 max_cmd = instance->max_fw_cmds;
        u32 defer_index;
        unsigned long flags;

        defer_index     = 0;
        spin_lock_irqsave(&instance->cmd_pool_lock, flags);
        for (i = 0; i < max_cmd; i++) {
                cmd = instance->cmd_list[i];
                if (cmd->sync_cmd == 1 || cmd->scmd) {
                        printk("megasas: moving cmd[%d]:%p:%d:%p on the defer queue as internal reset in progress.\n",
                                defer_index, cmd, cmd->sync_cmd, cmd->scmd);
                        if (!list_empty(&cmd->list)) {
                                printk("megaraid_sas: ERROR while moving this cmd:%p, %d %p, it was discovered on some list?\n", cmd, cmd->sync_cmd, cmd->scmd);
                                list_del_init(&cmd->list);
                        }
                        defer_index++;
                        list_add_tail(&cmd->list, &instance->internal_reset_pending_q);
                }
        }
        spin_unlock_irqrestore(&instance->cmd_pool_lock, flags);
}


static void 
process_fw_state_change_wq(struct work_struct *work) 
{
	struct megasas_instance *instance =
		container_of(work, struct megasas_instance, work_init);

        if (atomic_read(&instance->adprecovery) !=
				MEGASAS_ADPRESET_SM_INFAULT) {
		dev_err(&instance->pdev->dev,
			"%s:%d unexpected adapter recovery state 0x%x \n",
			__func__, __LINE__, atomic_read(&instance->adprecovery));
                return ;
        }

	if (atomic_read(&instance->adprecovery) == MEGASAS_ADPRESET_SM_INFAULT) {
		dev_info(&instance->pdev->dev,
			"%s:%d FW detected to be in fault state, restarting it...\n",
			__func__, __LINE__);

		instance->instancet->disable_intr(instance);
                atomic_set(&instance->fw_outstanding, 0);

                atomic_set(&instance->fw_reset_no_pci_access, 1);
                instance->instancet->adp_reset(instance, instance->reg_set);
                atomic_set(&instance->fw_reset_no_pci_access, 0 );

		dev_info(&instance->pdev->dev,
			"%s:%d HBA recovery state machine, state 2 starting...\n",
			__func__, __LINE__);

		/*waitting for about 30 second before start the second init*/
		ssleep(30);

                if (megasas_transition_to_ready(instance, 1))
                {
			dev_warn(&instance->pdev->dev,
				"%s:%d controller is not in ready state\n",
				__func__, __LINE__);
			atomic_set(&instance->fw_reset_no_pci_access, 1);
                        megaraid_sas_kill_hba(instance);
                        return ;
                }

		if ((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1064R)) ||
			(MR_PCI_ID(instance->pdev) == MR_PCI_ID_DELL(PCI_DEVICE_ID_DELL_PERC5)) ||
			(MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_VERDE_ZCR)))
		{
                        *instance->consumer = *instance->producer;
                } else {
                        *instance->consumer = 0;
                        *instance->producer = 0;
                }

                if (instance->cpx_supported) {
                        instance->cpx_request_queue->consumer_idx = instance->cpx_request_queue->producer_idx = 0;
                        if ( megasas_check_cpx_support( instance ) == 0  ){
                                if ( megasas_send_cpx_queue_data( instance ) ){
					dev_warn(&instance->pdev->dev,
						"%s:%d Sending cpx queue data to FW failed.\n",
						__func__, __LINE__);
                                        megasas_remove_cpx(instance);
                                }
                        }
                }

		// Transition the FW to operational state
		if (megasas_issue_init_mfi(instance)) {
			dev_warn(&instance->pdev->dev,
				"%s:%d failed to bring controller operatioanl\n",
				__func__, __LINE__);
			atomic_set(&instance->fw_reset_no_pci_access, 1);
                        megaraid_sas_kill_hba(instance);
                        return ;
		}

		/*
		 * Setting the adapter to OPERATIONAL at this point is very important. This would
		 * prevent other subsystems (reset, aen, and ioctls) to block till the recovery
		 * logic has run it's course
		 */
		atomic_set(&instance->adprecovery, MEGASAS_HBA_OPERATIONAL);
		instance->instancet->enable_intr(instance);

		dev_warn(&instance->pdev->dev,
			"%s:%d second stage of reset complete, FW is operational now.\n",
			__func__, __LINE__);
		megasas_issue_pending_cmds_again(instance);
		instance->issuepend_done = 1;
	}
	return ;
}

/**
 * megasas_deplete_reply_queue -	Processes all completed commands
 * @instance:				Adapter soft state
 * @alt_status:				Alternate status to be returned to
 * 					SCSI mid-layer instead of the status
 * 					returned by the FW
 * Note: this must be called with hba lock held
 */
static int
megasas_deplete_reply_queue(struct megasas_instance *instance, u8 alt_status)
{
	u32 mfiStatus;
	u32 abs_state, fw_state;

	// If the adapter is under a reset recovery, all interrupts coming from it must be acknowledged
	// if the consumer pointer value indicates so.
        if((mfiStatus = instance->instancet->check_reset(instance, instance->reg_set)) == 1) {
                return IRQ_HANDLED;
        }

	// Clear the interrupt on the HBA
	if((mfiStatus = instance->instancet->clear_intr(instance->reg_set)) == 0) {
		/* Hardware may not set outbound_intr_status in MSI-X mode */
		if (!instance->msix_vectors)
			return IRQ_NONE;
	}

	instance->mfiStatus = mfiStatus;

	// If the current soft state indicates an OPERATIONAL state _and_ now we have
	// detected state change, this should be FW FAULT case.
	if ((mfiStatus & MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE)) {
		abs_state = instance->instancet->read_fw_status_reg(instance->reg_set);
		fw_state = abs_state & MFI_STATE_MASK;

                if (fw_state != MFI_STATE_FAULT) {
						dev_info(&instance->pdev->dev,
							"%s:%d fw state while internal state change operational, fw state (0x%08x)\n",
							__func__, __LINE__, fw_state);
                }

               	if ((fw_state == MFI_STATE_FAULT) && (instance->disableOnlineCtrlReset == 0)){
			dev_info(&instance->pdev->dev,
				"%s:%d Found FW in FAULT state, will reset adapter - scratch pad (0x%08x) \n",
				__func__, __LINE__, abs_state);

			if ((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1064R)) ||
				(MR_PCI_ID(instance->pdev) == MR_PCI_ID_DELL(PCI_DEVICE_ID_DELL_PERC5)) ||
				(MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_VERDE_ZCR)))
			{
				*instance->consumer =
						cpu_to_le32(MEGASAS_ADPRESET_INPROG_SIGN);
			}


			instance->instancet->disable_intr(instance);
			atomic_set(&instance->adprecovery, MEGASAS_ADPRESET_SM_INFAULT);
			instance->issuepend_done = 0;

			// The pending commands are moved to a deferred list. We would pick commands up and
			// re-issue once the reset processing is over.
			atomic_set(&instance->fw_outstanding, 0);
			megasas_internal_reset_defer_cmds(instance);

			// Schedule a low-priorty thread to perform the function for current stage of
			// adapter reset state machine.
                        queue_work(megasas_ocr_wq, &instance->work_init);
                        return IRQ_HANDLED;
		}
		else {
			dev_info(&instance->pdev->dev, "%s:%d fw state while internal state changes," 
				"state:%x, disableOCR=%x\n",
				__func__, __LINE__, fw_state, instance->disableOnlineCtrlReset);
		}
	}

	// Schedule the tasklet for cmd completion
	tasklet_schedule(&instance->isr_tasklet);
	return IRQ_HANDLED;
}
/**
 * megasas_isr - isr entry point
 */
static irqreturn_t megasas_isr(int irq, void *devp)
{
	struct megasas_irq_context *irq_context = devp;
	struct megasas_instance *instance = irq_context->instance;
	unsigned long flags;
	irqreturn_t	rc;
      
	if (atomic_read(&instance->fw_reset_no_pci_access))
                return IRQ_HANDLED;

	spin_lock_irqsave(&instance->hba_lock, flags);
	rc =  megasas_deplete_reply_queue(instance, DID_OK);
	spin_unlock_irqrestore(&instance->hba_lock, flags);

	return rc;
}

/**
 * megasas_transition_to_ready -	Move the FW to READY state
 * @instance:				Adapter soft state
 *
 * During the initialization, FW passes can potentially be in any one of
 * several possible states. If the FW in operational, waiting-for-handshake
 * states, driver must take steps to bring it to ready state. Otherwise, it
 * has to wait for the ready state.
 */
int
megasas_transition_to_ready(struct megasas_instance* instance, int ocr)
{
	int i;
	u8 max_wait;
	u32 fw_state;
	u32 cur_state;
	u32 abs_state, curr_abs_state;
	

	abs_state = instance->instancet->read_fw_status_reg(instance->reg_set);
        fw_state = abs_state & MFI_STATE_MASK;


	if (fw_state != MFI_STATE_READY)
 		printk(KERN_INFO "megasas: Waiting for FW to come to ready"
 		       " state\n");

	while (fw_state != MFI_STATE_READY) {


		switch (fw_state) {

		case MFI_STATE_FAULT:
			printk(KERN_DEBUG "megasas: FW in FAULT state!!\n");
			if (ocr) {
				max_wait = MEGASAS_RESET_WAIT_TIME;
				cur_state = MFI_STATE_FAULT;
				break;
			} else
				return -ENODEV;

		case MFI_STATE_WAIT_HANDSHAKE:
			/*
			 * Set the CLR bit in inbound doorbell
			 */
 			if ((MR_PCI_ID(instance->pdev) ==
 				MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0073SKINNY)) ||
 				(MR_PCI_ID(instance->pdev) ==
 				 MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0071SKINNY)) ||
			    instance->ctrl_context)
				writel(MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
				  		&instance->reg_set->doorbell);
			else
				writel(
				    MFI_INIT_CLEAR_HANDSHAKE|MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_WAIT_HANDSHAKE;
			break;

		case MFI_STATE_BOOT_MESSAGE_PENDING:
 			if ((MR_PCI_ID(instance->pdev) ==
 				MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0073SKINNY)) ||
 				(MR_PCI_ID(instance->pdev) ==
 				 MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0071SKINNY)) ||
			    (instance->ctrl_context))
				writel(MFI_INIT_HOTPLUG, &instance->reg_set->doorbell);
			else
				writel(MFI_INIT_HOTPLUG,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BOOT_MESSAGE_PENDING;
			break;

		case MFI_STATE_OPERATIONAL:
			/*
			 * Bring it to READY state; assuming max wait 10 secs
			 */
			instance->instancet->disable_intr(instance);
 			if ((MR_PCI_ID(instance->pdev) ==
 				MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0073SKINNY)) ||
 				(MR_PCI_ID(instance->pdev) ==
 				MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0071SKINNY))  ||
				(instance->ctrl_context)) {

				writel(MFI_RESET_FLAGS,
					&instance->reg_set->doorbell);
				
				if (instance->ctrl_context) {
					for (i = 0; i < (10 * 1000); i++) {
						if (readl(&instance->reg_set->doorbell) & 1)
							msleep(1);
						else
							break;
					}
				}
			} else
				writel(MFI_RESET_FLAGS,
					&instance->reg_set->inbound_doorbell);

			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_OPERATIONAL;
			break;

		case MFI_STATE_UNDEFINED:
			/*
			 * This state should not last for more than 2 seconds
			 */
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_UNDEFINED;
			break;

		case MFI_STATE_BB_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_BB_INIT;
			break;

		case MFI_STATE_FW_INIT:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT;
			break;

		case MFI_STATE_FW_INIT_2:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FW_INIT_2;
			break;

		case MFI_STATE_DEVICE_SCAN:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_DEVICE_SCAN;
			break;

		case MFI_STATE_FLUSH_CACHE:
			max_wait = MEGASAS_RESET_WAIT_TIME;
			cur_state = MFI_STATE_FLUSH_CACHE;
			break;

		default:
			printk(KERN_DEBUG "megasas: Unknown state 0x%x\n",
			       fw_state);
			return -ENODEV;
		}

		/*
		 * The cur_state should not last for more than max_wait secs
		 */
		for (i = 0; i < (max_wait * 1000); i++) {
		curr_abs_state =
		instance->instancet->read_fw_status_reg(instance->reg_set);

			if (abs_state == curr_abs_state) {
				msleep(1);
			} else
				break;
		}

		/*
		 * Return error if fw_state hasn't changed after max_wait
		 */
		if (curr_abs_state == abs_state) {
			printk(KERN_DEBUG "FW state [%d] hasn't changed "
			       "in %d secs\n", fw_state, max_wait);
			return -ENODEV;
		}

		abs_state = curr_abs_state;
		fw_state = curr_abs_state & MFI_STATE_MASK;

	}
 	printk(KERN_INFO "megasas: FW now in Ready state\n");

	return 0;
}

/**
 * megasas_teardown_frame_pool -	Destroy the cmd frame DMA pool
 * @instance:				Adapter soft state
 */
static void megasas_teardown_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd = instance->max_mfi_cmds;
	struct megasas_cmd *cmd;

	if (!instance->frame_dma_pool)
		return;

	/*
	 * Return all frames to pool
	 */
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		if (cmd->frame)
			pci_pool_free(instance->frame_dma_pool, cmd->frame,
				      cmd->frame_phys_addr);

		if (cmd->sense)
			pci_pool_free(instance->sense_dma_pool, cmd->sense,
				      cmd->sense_phys_addr);
	}

	/*
	 * Now destroy the pool itself
	 */
	pci_pool_destroy(instance->frame_dma_pool);
	pci_pool_destroy(instance->sense_dma_pool);

	instance->frame_dma_pool = NULL;
	instance->sense_dma_pool = NULL;
}

/**
 * megasas_create_frame_pool -	Creates DMA pool for cmd frames
 * @instance:			Adapter soft state
 *
 * Each command packet has an embedded DMA memory buffer that is used for
 * filling MFI frame and the SG list that immediately follows the frame. This
 * function creates those DMA memory buffers for each command packet by using
 * PCI pool facility.
 */
static int megasas_create_frame_pool(struct megasas_instance *instance)
{
	int i;
	u32 max_cmd;
	u32 sge_sz;
	u32 total_sz;
	u32 frame_count;
	struct megasas_cmd *cmd;

	max_cmd = instance->max_mfi_cmds;

	/*
	 * Size of our frame is 64 bytes for MFI frame, followed by max SG
	 * elements and finally SCSI_SENSE_BUFFERSIZE bytes for sense buffer
	 */
	sge_sz = (IS_DMA64) ? sizeof(struct megasas_sge64) :
	    sizeof(struct megasas_sge32);

	if (instance->flag_ieee) {
		sge_sz = sizeof(struct megasas_sge_skinny);
	}

	/*
	 * For MFI controllers.
	 * max_num_sge = 60
	 * max_sge_sz  = 16 byte (sizeof megasas_sge_skinny)
	 * Totl 960 byte (15 MFI frame of 64 byte)
	 *
	 * Fusion adapter require only 3 extra frame.
	 * max_num_sge = 16 (defined as MAX_IOCTL_SGE)
	 * max_sge_sz  = 12 byte (sizeof  megasas_sge64)
	 * Total 192 byte (3 MFI frame of 64 byte)
	 */
	frame_count = instance->ctrl_context ? (3 + 1) : (15 + 1);
	total_sz = MEGAMFI_FRAME_SIZE * frame_count;
	/*
	 * Use DMA pool facility provided by PCI layer
	 */
	instance->frame_dma_pool = pci_pool_create("mfi_sg",
					instance->pdev, total_sz, 256, 0);

	if (!instance->frame_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup frame pool\n");
		return -ENOMEM;
	}

	instance->sense_dma_pool = pci_pool_create("mfi_sense",
						   instance->pdev, 128, 4, 0);

	if (!instance->sense_dma_pool) {
		printk(KERN_DEBUG "megasas: failed to setup sense pool\n");

		pci_pool_destroy(instance->frame_dma_pool);
		instance->frame_dma_pool = NULL;

		return -ENOMEM;
	}

	/*
	 * Allocate and attach a frame to each of the commands in cmd_list.
	 * By making cmd->index as the context instead of the &cmd, we can
	 * always use 32bit context regardless of the architecture
	 */
	for (i = 0; i < max_cmd; i++) {

		cmd = instance->cmd_list[i];

		cmd->frame = pci_pool_alloc(instance->frame_dma_pool,
					    GFP_KERNEL, &cmd->frame_phys_addr);

		cmd->sense = pci_pool_alloc(instance->sense_dma_pool,
					    GFP_KERNEL, &cmd->sense_phys_addr);

		/*
		 * megasas_teardown_frame_pool() takes care of freeing
		 * whatever has been allocated
		 */
		if (!cmd->frame || !cmd->sense) {
			printk(KERN_DEBUG "megasas: pci_pool_alloc failed \n");
			megasas_teardown_frame_pool(instance);
			return -ENOMEM;
		}

		memset(cmd->frame, 0, total_sz);

		cmd->frame->io.context = cpu_to_le32(cmd->index);

		/*
		 * Initialize pad_0 to 0, otherwise it could corrupt
		 * the value of context and cause FW crash
		 */
		cmd->frame->io.pad_0 = 0;
		if (!instance->ctrl_context && reset_devices)
			cmd->frame->hdr.cmd = MFI_CMD_INVALID;
	}

	return 0;
}

/**
 * megasas_free_cmds -	Free all the cmds in the free cmd pool
 * @instance:		Adapter soft state
 */
void megasas_free_cmds(struct megasas_instance *instance)
{
	int i;
	/* First free the MFI frame pool */
	megasas_teardown_frame_pool(instance);

	/* Free all the commands in the cmd_list */
	for (i = 0; i < instance->max_mfi_cmds; i++)

		kfree(instance->cmd_list[i]);

	/* Free the cmd_list buffer itself */
	kfree(instance->cmd_list);
	instance->cmd_list = NULL;

	INIT_LIST_HEAD(&instance->cmd_pool);
}

/**
 * megasas_alloc_cmds -	Allocates the command packets
 * @instance:		Adapter soft state
 *
 * Each command that is issued to the FW, whether IO commands from the OS or
 * internal commands like IOCTLs, are wrapped in local data structure called
 * megasas_cmd. The frame embedded in this megasas_cmd is actually issued to
 * the FW.
 *
 * Each frame has a 32-bit field called context (tag). This context is used
 * to get back the megasas_cmd from the frame when a frame gets completed in
 * the ISR. Typically the address of the megasas_cmd itself would be used as
 * the context. But we wanted to keep the differences between 32 and 64 bit
 * systems to the mininum. We always use 32 bit integers for the context. In
 * this driver, the 32 bit values are the indices into an array cmd_list.
 * This array is used only to look up the megasas_cmd given the context. The
 * free commands themselves are maintained in a linked list called cmd_pool.
 */
int megasas_alloc_cmds(struct megasas_instance *instance)
{
	int i;
	int j;
	u32 max_cmd;
	struct megasas_cmd *cmd;
	struct fusion_context *fusion;
	fusion = instance->ctrl_context;

	max_cmd = instance->max_mfi_cmds;

	/*
	 * instance->cmd_list is an array of struct megasas_cmd pointers.
	 * Allocate the dynamic array first and then allocate individual
	 * commands.
	 */
	instance->cmd_list = kcalloc(max_cmd, sizeof(struct megasas_cmd*), GFP_KERNEL);

	if (!instance->cmd_list) {
		printk(KERN_DEBUG "megasas: out of memory\n");
		return -ENOMEM;
	}

	memset(instance->cmd_list, 0, sizeof(struct megasas_cmd *) * max_cmd);

	for (i = 0; i < max_cmd; i++) {
		instance->cmd_list[i] = kmalloc(sizeof(struct megasas_cmd),
						GFP_KERNEL);

		if (!instance->cmd_list[i]) {

			for (j = 0; j < i; j++)
				kfree(instance->cmd_list[j]);

			kfree(instance->cmd_list);
			instance->cmd_list = NULL;

			return -ENOMEM;
		}
	}

	for (i = 0; i < max_cmd; i++) {
		cmd = instance->cmd_list[i];
		memset(cmd, 0, sizeof(struct megasas_cmd));
		cmd->index = i;
		cmd->scmd = NULL;
		cmd->instance = instance;

		list_add_tail(&cmd->list, &instance->cmd_pool);
	}

	/*
	 * Create a frame pool and assign one frame to each cmd
	 */
	if (megasas_create_frame_pool(instance)) {
		printk(KERN_DEBUG "megasas: Error creating frame DMA pool\n");
		megasas_free_cmds(instance);
	}

	return 0;
}

/*
 * dcmd_timeout_ocr_possible -	Check if OCR is possible based on Driver/FW state.
 * @instance:				Adapter soft state
 *
 * Return 0 for only Fusion adapter, if driver load/unload is not in progress
 * or FW is not under OCR.
 */
inline int 
dcmd_timeout_ocr_possible(struct megasas_instance *instance) {

	if (!instance->ctrl_context)
		return KILL_ADAPTER;
	else if (instance->unload || 
			test_bit(MEGASAS_FUSION_IN_RESET, &instance->reset_flags))
		return IGNORE_TIMEOUT;
	else
		return INITIATE_OCR;
}

/* Input:	 dcmd.opcode		 - MR_DCMD_PD_GET_INFO
  *		  dcmd.mbox.s[0]	  - deviceId for this physical drive
  *		  dcmd.sge IN		  - ptr to returned MR_PD_INFO structure
  *								If supportPointInTimeProgress is set, then MR_PROGRESS.elapsedSecsForLastPercent is returned
  *								Otherwise MR_PROGRESS.elapsedSecs is returned
  * Desc:	  Return the physical drive info structure
  * Status:  MFI_STAT_DEVICE_NOT_FOUND   - deviceId is invalid
  *
  * MR_DCMD_PD_GET_INFO					 = 0x02020000
  */


static int
megasas_get_pd_info(struct megasas_instance *instance, u16 device_id)
{
	int ret;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		dev_err(&instance->pdev->dev, "Failed to get cmd %s\n", __func__);
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	memset(instance->pd_info, 0, sizeof(*instance->pd_info));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->mbox.s[0] = cpu_to_le16(device_id);
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = cpu_to_le32(sizeof(struct MR_PD_INFO));
	dcmd->opcode = cpu_to_le32(MR_DCMD_PD_GET_INFO);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(instance->pd_info_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(sizeof(struct MR_PD_INFO));

	if (instance->ctrl_context && !instance->mask_interrupts)
		ret = megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS);
	else
		ret = megasas_issue_polled(instance, cmd);

	switch (ret) {
	case DCMD_SUCCESS:
		instance->pd_list[device_id].interface =
				instance->pd_info->state.ddf.pdType.intf;
		break;

	case DCMD_TIMEOUT:

		switch (dcmd_timeout_ocr_possible(instance)) {
		case INITIATE_OCR:
			cmd->flags |= DRV_DCMD_SKIP_REFIRE;
			megasas_reset_fusion(instance->host,
				MFI_IO_TIMEOUT_OCR);
			break;
		case KILL_ADAPTER:
			megaraid_sas_kill_hba(instance);
			break;
		case IGNORE_TIMEOUT:
			dev_info(&instance->pdev->dev, "Ignore DCMD timeout: %s %d \n",
				__func__, __LINE__);
			break;
		}

		break;
	}

	if (ret != DCMD_TIMEOUT)
		megasas_return_cmd(instance, cmd);

	return ret;
}

/*
 * megasas_get_pd_list_info -	Returns FW's pd_list structure
 * @instance:				Adapter soft state
 * @pd_list:				pd_list structure
 *
 * Issues an internal command (DCMD) to get the FW's controller PD
 * list structure.  This information is mainly used to find out SYSTEM
 * supported by the FW.
 */
static int
megasas_get_pd_list(struct megasas_instance *instance)
{
	int ret = 0, pd_index = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_PD_LIST *ci;
	struct MR_PD_ADDRESS *pd_addr;
	dma_addr_t ci_h = 0;

	if (instance->pd_list_not_supported) {
	   dev_info(&instance->pdev->dev, "MR_DCMD_PD_LIST_QUERY "
	       "not supported by firmware\n");
	   return ret;
	}

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas (get_pd_list): Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
		  MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for pd_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->mbox.b[0] = MR_PD_QUERY_TYPE_EXPOSED_TO_HOST;
	dcmd->mbox.b[1] = 0;
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = MFI_STAT_INVALID_STATUS;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = cpu_to_le32(MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST));
	dcmd->opcode = cpu_to_le32(MR_DCMD_PD_LIST_QUERY);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(ci_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST));

	if (instance->ctrl_context && !instance->mask_interrupts)
		ret = megasas_issue_blocked_cmd(instance, cmd,
			MFI_IO_TIMEOUT_SECS);
	else
		ret = megasas_issue_polled(instance, cmd);

	switch (ret) {
	case DCMD_FAILED:
		dev_info(&instance->pdev->dev, "MR_DCMD_PD_LIST_QUERY "
			"failed/not supported by firmware\n");

		if (instance->ctrl_context)
			megaraid_sas_kill_hba(instance);
		else
			instance->pd_list_not_supported = 1;
		break;
	case DCMD_TIMEOUT:

		switch (dcmd_timeout_ocr_possible(instance)) {
		case INITIATE_OCR:
			cmd->flags |= DRV_DCMD_SKIP_REFIRE;
			/* 
			 * DCMD failed from AEN path. 
			 * AEN path already hold reset_mutex to avoid PCI access
			 * while OCR is in progress.
			 */
			mutex_unlock(&instance->reset_mutex);
			megasas_reset_fusion(instance->host,
						MFI_IO_TIMEOUT_OCR);
			mutex_lock(&instance->reset_mutex);
			break;
		case KILL_ADAPTER:
			megaraid_sas_kill_hba(instance);
			break;
		case IGNORE_TIMEOUT:
			dev_info(&instance->pdev->dev, "Ignore DCMD timeout: %s %d \n",
				__func__, __LINE__);
			break;
		}
		
		break;

	case DCMD_SUCCESS:
		pd_addr = ci->addr;

		if ((le32_to_cpu(ci->count) > 
			(MEGASAS_MAX_PD_CHANNELS * MEGASAS_MAX_DEV_PER_CHANNEL)))
			break;

		memset(instance->local_pd_list, 0,
				MEGASAS_MAX_PD * sizeof(struct megasas_pd_list));

		for (pd_index = 0; pd_index < le32_to_cpu(ci->count); pd_index++) {
			instance->local_pd_list[le16_to_cpu(pd_addr->deviceId)].tid	=
					le16_to_cpu(pd_addr->deviceId);
			instance->local_pd_list[le16_to_cpu(pd_addr->deviceId)].driveType	=
					pd_addr->scsiDevType;
			instance->local_pd_list[le16_to_cpu(pd_addr->deviceId)].driveState	=
					MR_PD_STATE_SYSTEM;
			pd_addr++;
		}

		memcpy(instance->pd_list, instance->local_pd_list,
			sizeof(instance->pd_list));
		break;

	}

	pci_free_consistent(instance->pdev,
				MEGASAS_MAX_PD * sizeof(struct MR_PD_LIST),
				ci, ci_h);

	if (ret != DCMD_TIMEOUT)
		megasas_return_cmd(instance, cmd);

	return ret;
}
/**
 * megasas_get_ld_list_info -	Returns FW's ld_list structure
 * @instance:				Adapter soft state
 * @ld_list:				ld_list structure
 *
 * Issues an internal command (DCMD) to get the FW's controller PD
 * list structure.  This information is mainly used to find out SYSTEM
 * supported by the FW.
 */
static int
megasas_get_ld_list(struct megasas_instance *instance)
{
	int ret = 0, ld_index = 0, ids = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_LD_LIST *ci;
	dma_addr_t ci_h = 0;
	u32 ld_count;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas (megasas_get_ld_list): Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev, sizeof(struct MR_LD_LIST), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for megasas_get_ld_list\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	if(instance->supportmax256vd)
		dcmd->mbox.b[0] = 1;
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = MFI_STAT_INVALID_STATUS;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->data_xfer_len = cpu_to_le32(sizeof(struct MR_LD_LIST));
	dcmd->opcode = cpu_to_le32(MR_DCMD_LD_GET_LIST);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(ci_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(sizeof(struct MR_LD_LIST));
	dcmd->pad_0  = 0;

	if (instance->ctrl_context && !instance->mask_interrupts)
		ret = megasas_issue_blocked_cmd(instance, cmd,
			MFI_IO_TIMEOUT_SECS);
	else
		ret = megasas_issue_polled(instance, cmd);

	ld_count = le32_to_cpu(ci->ldCount);

	switch (ret) {
	case DCMD_FAILED:
		megaraid_sas_kill_hba(instance);
		break;
	case DCMD_TIMEOUT:
		
		switch (dcmd_timeout_ocr_possible(instance)) {
		case INITIATE_OCR:
			cmd->flags |= DRV_DCMD_SKIP_REFIRE;
			/* 
			 * DCMD failed from AEN path. 
			 * AEN path already hold reset_mutex to avoid PCI access
			 * while OCR is in progress.
			 */
			mutex_unlock(&instance->reset_mutex);
			megasas_reset_fusion(instance->host,
						MFI_IO_TIMEOUT_OCR);
			mutex_lock(&instance->reset_mutex);
			break;
		case KILL_ADAPTER:
			megaraid_sas_kill_hba(instance);
			break;
		case IGNORE_TIMEOUT:
			dev_info(&instance->pdev->dev, "Ignore DCMD timeout: %s %d \n",
				__func__, __LINE__);
			break;
		}

		break;

	case DCMD_SUCCESS:
		BUG_ON(instance->fw_supported_vd_count > MAX_LOGICAL_DRIVES_EXT);
		if (ld_count > instance->fw_supported_vd_count)
			break;

		instance->CurLdCount = ci->ldCount;
		memset(instance->ld_ids, 0xff, MAX_LOGICAL_DRIVES_EXT);

		for (ld_index = 0; ld_index < ld_count; ld_index++) {
			if (ci->ldList[ld_index].state != 0) {
				ids = ci->ldList[ld_index].ref.targetId;
				instance->ld_ids[ids] = ci->ldList[ld_index].ref.targetId;
			}
		}

		break;
	}

	pci_free_consistent(instance->pdev, sizeof(struct MR_LD_LIST), ci, ci_h); 

	if (ret != DCMD_TIMEOUT)
		megasas_return_cmd(instance, cmd);

	return ret;
}

/**
 * megasas_ld_list_query -	Returns FW's ld_list structure
 * @instance:				Adapter soft state
 * @ld_list:				ld_list structure
 * return:				0: DCMD_SUCCESS
 * 					1: DCMD_TIMEOUT
 * 					2: DCMD_FAILED
 *
 * Issues an internal command (DCMD) to get the FW's controller PD
 * list structure.  This information is mainly used to find out SYSTEM
 * supported by the FW.
 */
static int
megasas_ld_list_query(struct megasas_instance *instance, u8 query_type)
{
	int ret = 0, ld_index = 0, ids = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_LD_TARGETID_LIST *ci;
	dma_addr_t ci_h = 0;
	u32 tgtid_count;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_WARNING
		       "megasas:(megasas_ld_list_query): Failed to get cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				  sizeof(struct MR_LD_TARGETID_LIST), &ci_h);

	if (!ci) {
		printk(KERN_WARNING
		       "megasas: Failed to alloc mem for ld_list_query\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->mbox.b[0] = query_type;
	if(instance->supportmax256vd)
		dcmd->mbox.b[2] = 1;

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = MFI_STAT_INVALID_STATUS;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->data_xfer_len = cpu_to_le32(sizeof(struct MR_LD_TARGETID_LIST));
	dcmd->opcode = cpu_to_le32(MR_DCMD_LD_LIST_QUERY);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(ci_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(sizeof(struct MR_LD_TARGETID_LIST));
	dcmd->pad_0  = 0;

	if (instance->ctrl_context && !instance->mask_interrupts)
		ret = megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS);
	else
		ret = megasas_issue_polled(instance, cmd);

	switch (ret) {
	case DCMD_FAILED:
		dev_info(&instance->pdev->dev,
			"MR_DCMD_LD_LIST_QUERY not supported by fw - %s %d \n", __func__, __LINE__);
		ret = megasas_get_ld_list(instance);
		break;
	case DCMD_TIMEOUT:
		switch (dcmd_timeout_ocr_possible(instance)) {
		case INITIATE_OCR:
			cmd->flags |= DRV_DCMD_SKIP_REFIRE;
			/* 
			 * DCMD failed from AEN path. 
			 * AEN path already hold reset_mutex to avoid PCI access
			 * while OCR is in progress.
			 */
			mutex_unlock(&instance->reset_mutex);
			megasas_reset_fusion(instance->host,
						MFI_IO_TIMEOUT_OCR);
			mutex_lock(&instance->reset_mutex);
			break;
		case KILL_ADAPTER:
			megaraid_sas_kill_hba(instance);
			break;
		case IGNORE_TIMEOUT:
			dev_info(&instance->pdev->dev, "Ignore DCMD timeout: %s %d \n",
				__func__, __LINE__);
			break;
		}

		break;
	case DCMD_SUCCESS:
		tgtid_count = le32_to_cpu(ci->count);

		if ((tgtid_count > (instance->fw_supported_vd_count)))
			break;

		memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);
		for (ld_index = 0; ld_index < tgtid_count; ld_index++) {
			ids = ci->targetId[ld_index];
			instance->ld_ids[ids] = ci->targetId[ld_index];
		}

		break;
	}

	pci_free_consistent(instance->pdev, sizeof(struct MR_LD_TARGETID_LIST),
		    ci, ci_h);

	if (ret != DCMD_TIMEOUT)
		megasas_return_cmd(instance, cmd);
	
	return ret;
}

/*
 * megasas_update_ext_vd_details : Update details w.r.t Extended VD
 * instance			 : Controller's instance
*/
static void megasas_update_ext_vd_details(struct megasas_instance *instance)
{
	struct fusion_context *fusion;
	u32 ventura_map_sz;

	fusion = instance->ctrl_context;
	/* For MFI based controllers return dummy success */
	if (!fusion)
		return;

	instance->supportmax256vd =
		instance->ctrl_info->adapterOperations3.supportMaxExtLDs;
	/* Below is additional check to address future FW enhancement */
	if (instance->ctrl_info->max_lds > 64)
		instance->supportmax256vd = 1;

	instance->drv_supported_vd_count = MEGASAS_MAX_LD_CHANNELS
					* MEGASAS_MAX_DEV_PER_CHANNEL;
	instance->drv_supported_pd_count = MEGASAS_MAX_PD_CHANNELS
					* MEGASAS_MAX_DEV_PER_CHANNEL;
	if (instance->supportmax256vd) {
		instance->fw_supported_vd_count = MAX_LOGICAL_DRIVES_EXT;
		instance->fw_supported_pd_count = MAX_PHYSICAL_DEVICES;
	} else {
		instance->fw_supported_vd_count = MAX_LOGICAL_DRIVES;
		instance->fw_supported_pd_count = MAX_PHYSICAL_DEVICES;
	}
	dev_info(&instance->pdev->dev,
		"firmware type\t: %s\n",
		instance->supportmax256vd ? "Extended VD(240 VD)firmware" :
		"Legacy(64 VD) firmware");
	if (instance->maxRaidMapSize) {
		ventura_map_sz = instance->maxRaidMapSize * //FW writes value 3
						MR_MIN_MAP_SIZE; //64k
		fusion->current_map_sz = ventura_map_sz;
		fusion->max_map_sz = ventura_map_sz;
	} else {
		fusion->old_map_sz =  sizeof(MR_FW_RAID_MAP) +
					(sizeof(MR_LD_SPAN_MAP) *
					(instance->fw_supported_vd_count - 1));
		fusion->new_map_sz =  sizeof(MR_FW_RAID_MAP_EXT);

		fusion->max_map_sz = max(fusion->old_map_sz, fusion->new_map_sz);

		if (instance->supportmax256vd)
			fusion->current_map_sz = fusion->new_map_sz;
		else
			fusion->current_map_sz = fusion->old_map_sz;
	}
	//irrespective of FW raid maps, driver raid map is constant
	fusion->drv_map_sz = sizeof(MR_DRV_RAID_MAP_ALL); 
#if VD_EXT_DEBUG	
	dev_info(&instance->pdev->dev, "instance->maxRaidMapSize 0x%x \n ",
			instance->maxRaidMapSize);
	dev_info(&instance->pdev->dev,
				"new_map_sz = 0x%x, old_map_sz = 0x%x, "
				"ventura_map_sz = 0x%x, current_map_sz = 0x%x "
				"fusion->drv_map_sz =0x%x, size of driver raid map 0x%lx\n",
				fusion->new_map_sz, fusion->old_map_sz,
				ventura_map_sz, fusion->current_map_sz,
				fusion->drv_map_sz, sizeof(MR_DRV_RAID_MAP_ALL));
#endif
}

/**
 * megasas_get_bios_data  -	Returns FW's mr_bios_data structure
 * @instance:			Adapter structure
 *
 * Issues an internal command (DCMD) to get the FW's controller BIOS
 * data structure.  This information is mainly used to find the
 * boot drive and its target id.
 */

static int
megasas_get_bios_data(struct megasas_instance *instance)
{
	int ret = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct MR_BIOS_DATA *ci;
	struct megasas_drive_order *drv_odr;
	dma_addr_t ci_h = 0;

	drv_odr = &instance->drive_order;
	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		dev_info(&instance->pdev->dev, "%s : "
		    "Failed to get cmd\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
	    sizeof(struct MR_BIOS_DATA), &ci_h);

	if (!ci) {
		dev_info(&instance->pdev->dev, "%s : "
		    "Failed to alloc mem for get_bios_data\n", __func__);
		ret = -ENOMEM;
		goto cmd_free;
	}

	memset(ci, 0, sizeof(struct MR_BIOS_DATA));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0xFF;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->data_xfer_len = cpu_to_le32(sizeof(struct MR_BIOS_DATA));
	dcmd->opcode = cpu_to_le32(MR_DCMD_CTRL_BIOS_DATA_GET);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(ci_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(sizeof(struct MR_BIOS_DATA));
	dcmd->pad_0  = 0;

	if (instance->ctrl_context && !instance->mask_interrupts)
		ret = megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS);
	else
		ret = megasas_issue_polled(instance, cmd);

	switch(ret) {

	case DCMD_SUCCESS:
			/* check if boot drive is there */
			if (le16_to_cpu(ci->bootTargetId) != MEGASAS_INVALID_TARGET_ID) {
				drv_odr->boot_fw_tgt_id = le16_to_cpu(ci->bootTargetId);
				drv_odr->boot_drive_is_pd = ci->bootDeviceIsPD;

				if (drv_odr->boot_drive_is_pd) {
					/* System PD Channel id set to 0 or 1 */
					drv_odr->boot_drive_channel_id = 
						drv_odr->boot_fw_tgt_id/
						MEGASAS_MAX_DEV_PER_CHANNEL;

					/* System PD Target id set to 0 - 127 */
					drv_odr->boot_drive_tgt_id = 
						drv_odr->boot_fw_tgt_id - 
						(drv_odr->boot_drive_channel_id * 
						MEGASAS_MAX_DEV_PER_CHANNEL); 
				} else {
					/* LD Channel id set to 2 or 3 */
					drv_odr->boot_drive_channel_id = 
						MEGASAS_MAX_PD_CHANNELS + 
						(drv_odr->boot_fw_tgt_id/
						MEGASAS_MAX_DEV_PER_CHANNEL); 

					/* LD Target id set to 0 - 127 */
					drv_odr->boot_drive_tgt_id = 
						drv_odr->boot_fw_tgt_id - 
						((drv_odr->boot_drive_channel_id - 
						MEGASAS_MAX_PD_CHANNELS) * 
						MEGASAS_MAX_DEV_PER_CHANNEL); 
				}
			}
			break;
	case DCMD_TIMEOUT:
		switch (dcmd_timeout_ocr_possible(instance)) {
		case INITIATE_OCR:
			cmd->flags |= DRV_DCMD_SKIP_REFIRE;
			/*
			 * DCMD failed from AEN path.
			 * AEN path already hold reset_mutex to avoid PCI access
			 * while OCR is in progress.
			 */
			megasas_reset_fusion(instance->host,
			MFI_IO_TIMEOUT_OCR);
			break;
		case KILL_ADAPTER:
			megaraid_sas_kill_hba(instance);
			break;
		case IGNORE_TIMEOUT:
			dev_info(&instance->pdev->dev, "Ignore DCMD timeout: %s %d \n",
						__func__, __LINE__);
			break;

		}
		break;
	case DCMD_FAILED: 
		dev_info(&instance->pdev->dev, "%s : "
		    "Failed with return(%d)\n", __func__, ret);
	}

	pci_free_consistent(instance->pdev, sizeof(struct MR_BIOS_DATA),
	    ci, ci_h);

cmd_free:
	if (ret != DCMD_TIMEOUT)
                megasas_return_cmd(instance, cmd); 
out:
	return ret;
}

/**
 * megasas_get_controller_info -	Returns FW's controller structure
 * @instance:				Adapter soft state
 *
 * Issues an internal command (DCMD) to get the FW's controller structure.
 * This information is mainly used to find out the maximum IO transfer per
 * command supported by the FW.
 */
int
megasas_get_ctrl_info(struct megasas_instance *instance)
{
	int ret = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_ctrl_info *ci;
	struct megasas_ctrl_info *ctrl_info;
	dma_addr_t ci_h = 0;

	ctrl_info = instance->ctrl_info;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a free cmd\n");
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;

	ci = pci_alloc_consistent(instance->pdev,
				  sizeof(struct megasas_ctrl_info), &ci_h);

	if (!ci) {
		printk(KERN_DEBUG "Failed to alloc mem for ctrl info\n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(ci, 0, sizeof(*ci));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = MFI_STAT_INVALID_STATUS;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = cpu_to_le32(sizeof(struct megasas_ctrl_info));
	dcmd->opcode = cpu_to_le32(MR_DCMD_CTRL_GET_INFO);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(ci_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(sizeof(struct megasas_ctrl_info));
	/*
	dcmd.mbox.b[0]  0 - supports MAX_API_LOGICAL_DRIVES_LEGACY lds and MAX_API_ARRAYS_LEGACY
			1 - supports MAX_API_LOGICAL_DRIVES_EXT lds MAX_API_ARRAYS_EXT
	*/
	dcmd->mbox.b[0] = 1;

	if (instance->ctrl_context && !instance->mask_interrupts)
		ret = megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS);
	else
		ret = megasas_issue_polled(instance, cmd);

	switch(ret)
	{
		case DCMD_SUCCESS:
				memcpy(ctrl_info, ci, sizeof(struct megasas_ctrl_info));
				/* Save required controller information in
				 * CPU endianness format.
				 */
				le32_to_cpus((u32 *)&ctrl_info->properties.OnOffProperties);
				le32_to_cpus((u32 *)&ctrl_info->adapterOperations2);
				le32_to_cpus((u32 *)&ctrl_info->adapterOperations3);
				le16_to_cpus((u16 *)&ctrl_info->adapterOperations4);

				/* Update the latest Ext VD info.
				 * From Init path, store current firmware details.
				 * From OCR path, detect any firmware properties changes.
				 * in case of Firmware upgrade without system reboot.
				 */
				megasas_update_ext_vd_details(instance);
				instance->use_seqnum_jbod_fp =
					ctrl_info->adapterOperations3.useSeqNumJbodFP;
				instance->support_morethan256jbod = 
					ctrl_info->adapterOperations4.supportPdMapTargetId;

				/*Check whether controller is iMR or MR */
				instance->is_imr = (ctrl_info->memory_size ? 0 : 1);
				dev_info(&instance->pdev->dev,
						"controller type\t: %s(%dMB)\n",
						instance->is_imr ? "iMR" : "MR",
						le16_to_cpu(ctrl_info->memory_size));
				
				instance->disableOnlineCtrlReset =
					ctrl_info->properties.OnOffProperties.disableOnlineCtrlReset;
				instance->secure_jbod_support =
					ctrl_info->adapterOperations3.supportSecurityonJBOD;
				dev_info(&instance->pdev->dev, "Online Controller Reset(OCR)\t: %s\n",
						instance->disableOnlineCtrlReset ? "Disabled" : "Enabled");
				dev_info(&instance->pdev->dev, "Secure JBOD support\t: %s\n",
						instance->secure_jbod_support ? "Yes" : "No");
				break;

		case DCMD_TIMEOUT:
				switch (dcmd_timeout_ocr_possible(instance)) {
				case INITIATE_OCR:
					cmd->flags |= DRV_DCMD_SKIP_REFIRE;
					megasas_reset_fusion(instance->host,
							MFI_IO_TIMEOUT_OCR);
					break;
				case KILL_ADAPTER:
					megaraid_sas_kill_hba(instance);
					break;
				case IGNORE_TIMEOUT:
					dev_info(&instance->pdev->dev, "Ignore DCMD timeout: %s %d \n",
						__func__, __LINE__);
					break;
				}
				break;
		case DCMD_FAILED:
			megaraid_sas_kill_hba(instance);
			break;

	}	

	pci_free_consistent(instance->pdev, sizeof(struct megasas_ctrl_info),
			    ci, ci_h);

	megasas_return_cmd(instance, cmd);


	return ret;
}


/**
 * megasas_set_crash_dump_params -	Sends address of crash dump DMA buffer
 * 					to firmware
 * 					
 * @instance:				Adapter soft state
 * @crash_buf_state		- 	tell FW to turn ON/OFF crash dump feature
    					MR_CRASH_BUF_TURN_OFF = 0
    					MR_CRASH_BUF_TURN_ON = 1
 * @return 0 on success non-zero on failure.
 * Issues an internal command (DCMD) to set parameters for crash dump feature.
 * Driver will send address of crash dump DMA buffer and set mbox to tell FW
 * that driver supports crash dump feature. This DCMD will be sent only if 
 * crash dump feature is supported by the FW.
 * 
 */
int
megasas_set_crash_dump_params(struct megasas_instance *instance, u8 crash_buf_state)
		      
{
	int ret = 0;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a free cmd\n");
		return -ENOMEM;
	}
	

	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);
	dcmd->mbox.b[0] = crash_buf_state;
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = MFI_STAT_INVALID_STATUS;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_NONE);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = cpu_to_le32(CRASH_DMA_BUF_SIZE);
	dcmd->opcode = cpu_to_le32(MR_DCMD_CTRL_SET_CRASH_DUMP_PARAMS);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(instance->crash_dump_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(CRASH_DMA_BUF_SIZE);

	if (instance->ctrl_context && !instance->mask_interrupts)
		ret = megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS);
	else
		ret = megasas_issue_polled(instance, cmd);
	
	if (ret == DCMD_TIMEOUT) {
		switch (dcmd_timeout_ocr_possible(instance)) {
		case INITIATE_OCR:
			cmd->flags |= DRV_DCMD_SKIP_REFIRE;
			megasas_reset_fusion(instance->host,
					MFI_IO_TIMEOUT_OCR);
			break;
		case KILL_ADAPTER:
			megaraid_sas_kill_hba(instance);
			break;
		case IGNORE_TIMEOUT:
			dev_info(&instance->pdev->dev, "Ignore DCMD timeout: %s %d \n",
				__func__, __LINE__);
			break;
		}
	} else
		megasas_return_cmd(instance, cmd);

	return ret;
}

/**
 * megasas_check_cpx_support : Tries to get the host memory address if fails then cpx
				not supported else cpx supported.
 * @instance:		Adapter soft state
 *
 * @return 0 on success non-zero on failure.
 */
static int megasas_check_cpx_support( struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct mr_cpx_init_data *cpx_init_data;
	dma_addr_t cpx_init_data_h;
	int retval = 0;
		
	cmd = megasas_get_cmd(instance);
	if (!cmd) {
		printk(KERN_DEBUG "megasas (get_host_mem_addr): Failed to get cmd\n");
		return -ENOMEM;
	}

	cpx_init_data = pci_alloc_consistent(instance->pdev, sizeof( struct mr_cpx_init_data), &cpx_init_data_h);
	if (cpx_init_data == NULL) {
		printk(KERN_DEBUG "Failed to alloc mem for cpx_init_data. \n");
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}


	dcmd = &cmd->frame->dcmd;
	dcmd->flags = 0;
	dcmd->opcode = MR_DCMD_CTRL_MISC_CPX_INIT_DATA_GET;
	dcmd->data_xfer_len  = sizeof(struct mr_cpx_init_data );
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status  = 0xff;
	dcmd->sge_count = 1;
	dcmd->sgl.sge32[0].phys_addr = cpx_init_data_h;
	dcmd->sgl.sge32[0].length = sizeof(struct mr_cpx_init_data);

	retval = megasas_issue_polled ( instance, cmd );
	
	if ( retval == 0 && cmd->frame->hdr.cmd_status == 0  ){
		instance->host_mem_phys = cpx_init_data->phys_addr_cache_buf;
		instance->host_mem_len  = cpx_init_data->size;
		instance->cpx_dscrptr_cnt = cpx_init_data->cpx_desc_count;
		if ( instance->host_mem_len == 0 || instance->host_mem_phys == 0 || ! instance->cpx_dscrptr_cnt ){
			printk("Got host_mem_len = 0 OR host_mem_phys as NULL OR cpx_descriptor count = 0 \n");
			retval = 1;
		}
	} else {
		printk("megasas: cpx is not supported.\n");
		retval = 1;
	}
	
	instance->cpx_supported = (retval?0:1);
	
	pci_free_consistent(instance->pdev, sizeof(struct mr_cpx_init_data), cpx_init_data, cpx_init_data_h);
	megasas_return_cmd(instance, cmd);
	
	return retval;
}

/**
 * megasas_send_cpx_queue_data : Sends the queue setup info to fw.
 * @instance:		Adapter soft state
 *
 * @return 0 on success non-zero on failure.
 */

static int megasas_send_cpx_queue_data( struct megasas_instance *instance )
{

	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	int retval = 0;
	
	cmd = megasas_get_cmd(instance);
	if (!cmd) {
		printk(KERN_DEBUG "megasas (get_host_mem_addr): Failed to get cmd\n");
		return -ENOMEM;
	}

	//initialize dcmd data
	dcmd = &cmd->frame->dcmd;
	dcmd->flags = 0;
	dcmd->opcode = MR_DCMD_CTRL_MISC_CPX_QUEUE_DATA;
	dcmd->data_xfer_len  = sizeof( struct mr_cpx_queue_data );
	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status  = 0xff;
	dcmd->sge_count = 1;
	dcmd->mbox.w[0] = instance->cpx_request_queue_h;
	dcmd->mbox.w[1] = 0;
	dcmd->mbox.w[2] = instance->cpx_dscrptr_cnt;

	retval = megasas_issue_polled ( instance, cmd );
	
	megasas_return_cmd( instance, cmd);
	
	if ( retval == 0 ){
		instance->cpx_request_queue->consumer_idx = instance->cpx_request_queue->producer_idx = 0;
		instance->cpx_supported = 1;
	}
	
	return retval;
}

static u32 megasas_get_cpx_mem_len( u16 max_fw_cmds )
{
	return (sizeof( struct mr_cpx_request_queue ) + sizeof( union mr_cpx_descriptor ) * ( max_fw_cmds) );
}

static u32 megasas_remove_cpx( struct megasas_instance *instance)
{
		if ( instance->host_mem_virt )
			iounmap(instance->host_mem_virt);
		if ( instance->cpx_request_queue )
			pci_free_consistent(instance->pdev, megasas_get_cpx_mem_len( instance->cpx_dscrptr_cnt),
					    instance->cpx_request_queue,instance->cpx_request_queue_h );
		instance->cpx_supported = 0;
	
	return 0;
}

/* should have host_mem_phys intialized before calling this function.*/	
static int megasas_init_cpx( struct megasas_instance *instance)
{

	//map address
	instance->host_mem_virt = ioremap(instance->host_mem_phys, instance->host_mem_len);
	if ( instance->host_mem_virt == NULL ){
		printk("megasas: Failed to ioremap host memory.\n");
		goto error_unload;
	}
		
	//allocate memory for indices and descriptors for reply and response array's
	instance->cpx_request_queue = pci_alloc_consistent( instance->pdev, megasas_get_cpx_mem_len (instance->cpx_dscrptr_cnt), &instance->cpx_request_queue_h );
	if ( instance->cpx_request_queue == NULL ){
		printk(KERN_ERR "megasas: Out of DMA memory for cpx operations.\n");
		goto error_unload;
	}
		
	//initialize queues
	instance->cpx_dscrptr = (union mr_cpx_descriptor *)((u8*)instance->cpx_request_queue + (  sizeof(instance->cpx_request_queue->consumer_idx)*2  ));

	//send data to fw.
	if ( megasas_send_cpx_queue_data( instance ) ){
		printk("megasas: Sending cpx queue data to FW failed.\n");
		goto error_unload;
	}
			
	return 0;

error_unload:
	megasas_remove_cpx( instance );
	return -1;
}

/**
 * megasas_issue_init_mfi -	Initializes the FW
 * @instance:		Adapter soft state
 *
 * Issues the INIT MFI cmd
 */
static int
megasas_issue_init_mfi(struct megasas_instance *instance)
{
	u32 context;

	struct megasas_cmd *cmd;

	struct megasas_init_frame *init_frame;
	struct megasas_init_queue_info *initq_info;
	dma_addr_t init_frame_h;
	dma_addr_t initq_info_h;

	/*
	 * Prepare a init frame. Note the init frame points to queue info
	 * structure. Each frame has SGL allocated after first 64 bytes. For
	 * this frame - since we don't need any SGL - we use SGL's space as
	 * queue info structure
	 *
	 * We will not get a NULL command below. We just created the pool.
	 */
	cmd = megasas_get_cmd(instance);

	init_frame = (struct megasas_init_frame *)cmd->frame;
	initq_info = (struct megasas_init_queue_info *)
		((unsigned long)init_frame + 64);

	init_frame_h = cmd->frame_phys_addr;
	initq_info_h = init_frame_h + 64;

	context = init_frame->context;
	memset(init_frame, 0, MEGAMFI_FRAME_SIZE);
	memset(initq_info, 0, sizeof(struct megasas_init_queue_info));
	init_frame->context = context;

	initq_info->reply_queue_entries = cpu_to_le32(instance->max_fw_cmds + 1);
	initq_info->reply_queue_start_phys_addr_lo = cpu_to_le32(instance->reply_queue_h);
 
	initq_info->producer_index_phys_addr_lo = cpu_to_le32(instance->producer_h);
	initq_info->consumer_index_phys_addr_lo = cpu_to_le32(instance->consumer_h);

	init_frame->cmd = MFI_CMD_INIT;
	init_frame->cmd_status = MFI_STAT_INVALID_STATUS;

	if (instance->verbuf) {
		snprintf((char *)instance->verbuf, strlen(MEGASAS_VERSION) + 2, "%s",
			 MEGASAS_VERSION);

		if (reset_devices)
			strcat((char *)instance->verbuf, "-kdump");

		init_frame->driver_ver_lo = cpu_to_le32(instance->verbuf_h);
		init_frame->driver_ver_hi = 0;
	}


	init_frame->queue_info_new_phys_addr_lo =
		cpu_to_le32(lower_32_bits(initq_info_h));
	init_frame->queue_info_new_phys_addr_hi =
		cpu_to_le32(upper_32_bits(initq_info_h));
  
	init_frame->data_xfer_len = cpu_to_le32(sizeof(struct megasas_init_queue_info));

	/*
	 * disable the intr before firing the init frame to FW
	 */
	instance->instancet->disable_intr(instance);

	/*
	 * Issue the init frame in polled mode
	 */

	if (megasas_issue_polled(instance, cmd)) {
		printk(KERN_ERR "megasas: Failed to init firmware\n");
		megasas_return_cmd(instance, cmd);
		goto fail_fw_init;
	}

	megasas_return_cmd(instance, cmd);

	return 0;

fail_fw_init:
	return -EINVAL;
}

static u32
megasas_init_adapter_mfi(struct megasas_instance *instance)
{
	struct megasas_register_set __iomem *reg_set;
	u32 context_sz;
	u32 reply_q_sz;

	reg_set = instance->reg_set;

	/*
	 * Get various operational parameters from status register
	 */
	instance->max_fw_cmds = instance->instancet->read_fw_status_reg(reg_set) & 0x00FFFF;
	/*
	 * Reduce the max supported cmds by 1. This is to ensure that the
	 * reply_q_sz (1 more than the max cmd that driver may send)
	 * does not exceed max cmds that the FW can support
	 */
	instance->max_fw_cmds = instance->max_fw_cmds-1;
	instance->max_mfi_cmds = instance->max_fw_cmds;
	instance->max_num_sge = (instance->instancet->read_fw_status_reg(reg_set) & 0xFF0000) >> 
					0x10;
	/*
	 * For MFI skinny adapters, MEGASAS_SKINNY_INT_CMDS commands
	 * are reserved for IOCTL + driver's internal DCMDs.
	 */
 	if ((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0073SKINNY)) ||
 		(MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0071SKINNY))) {
		instance->max_scsi_cmds = (instance->max_fw_cmds -
			MEGASAS_SKINNY_INT_CMDS);
		sema_init(&instance->ioctl_sem, MEGASAS_SKINNY_INT_CMDS);
	} else {
		instance->max_scsi_cmds = (instance->max_fw_cmds -
			MEGASAS_INT_CMDS);
		sema_init(&instance->ioctl_sem, (MEGASAS_INT_CMDS - 5));
	}

	instance->cur_can_queue = instance->max_scsi_cmds;
	/*
	 * Create a pool of commands
	 */
	if (megasas_alloc_cmds(instance))
		goto fail_alloc_cmds;

	/*
	 * Allocate memory for reply queue. Length of reply queue should
	 * be _one_ more than the maximum commands handled by the firmware.
	 *
	 * Note: When FW completes commands, it places corresponding contex
	 * values in this circular reply queue. This circular queue is a fairly
	 * typical producer-consumer queue. FW is the producer (of completed
	 * commands) and the driver is the consumer.
	 */
	context_sz = sizeof(u32);
	reply_q_sz = context_sz * (instance->max_fw_cmds + 1);

	instance->reply_queue = pci_alloc_consistent(instance->pdev,
						     reply_q_sz,
						     &instance->reply_queue_h);

	if (!instance->reply_queue) {
		printk(KERN_DEBUG "megasas: Out of DMA mem for reply queue\n");
		goto fail_reply_queue;
	}

        if ( megasas_check_cpx_support( instance ) == 0  )
                if ( megasas_init_cpx( instance ) )
                        printk("Error in initilizing cpx.\n");

	if (megasas_issue_init_mfi(instance))
		goto fail_fw_init;

	BUG_ON(!instance->ctrl_info);	
	if (megasas_get_ctrl_info(instance)) {
		dev_err(&instance->pdev->dev,
			"Failed from %s %d\n", __func__, __LINE__);
		goto fail_fw_init;
	}

	instance->fw_support_ieee = 0; 
	instance->fw_support_ieee = (instance->instancet->read_fw_status_reg(reg_set) & 0x04000000); 

	if (megasas_dbg_lvl)
		printk("megasas_init: fw_support_ieee=%d\n", instance->fw_support_ieee);
	if (instance->fw_support_ieee)
		instance->flag_ieee = 1;

	return 0;

fail_fw_init:

	pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);

fail_reply_queue:
	megasas_free_cmds(instance);

fail_alloc_cmds:
	return 1;
}


/**
 * megasas_setup_jbod_map -	setup jbod map for FP seq_number.
 * @instance:				Adapter soft state
 * @is_probe:				Driver probe check
 *
 * Return 0 on success.
 */
void
megasas_setup_jbod_map(struct megasas_instance *instance)
{
	int i;
	struct fusion_context *fusion = instance->ctrl_context;
	u32 pd_seq_map_sz;
	pd_seq_map_sz = sizeof(struct MR_PD_CFG_SEQ_NUM_SYNC) +
		(sizeof(struct MR_PD_CFG_SEQ) * (MAX_PHYSICAL_DEVICES - 1));

	if (reset_devices || !fusion ||
		!instance->ctrl_info->adapterOperations3.useSeqNumJbodFP) {
		dev_info(&instance->pdev->dev,
			"Jbod map is not supported %s %d\n",
			__func__, __LINE__);
		instance->use_seqnum_jbod_fp = 0;
		return;
	}

	if (fusion->pd_seq_sync[0])
		goto skip_alloc;

	for (i = 0; i < 2; i++) {
		fusion->pd_seq_sync[i] = dma_alloc_coherent
			(&instance->pdev->dev, pd_seq_map_sz,
			&fusion->pd_seq_phys[i], GFP_KERNEL);
		if (!fusion->pd_seq_sync[i]) {
			dev_err(&instance->pdev->dev,
				"Failed to allocate memory from %s %d\n",
				__func__, __LINE__);
			if (i == 1) {
				dma_free_coherent(&instance->pdev->dev,
					pd_seq_map_sz, fusion->pd_seq_sync[0],
					fusion->pd_seq_phys[0]);
				fusion->pd_seq_sync[0] = NULL;
			}
			instance->use_seqnum_jbod_fp = 0;
			return;
		}
	}

skip_alloc:
	if (!megasas_sync_pd_seq_num(instance, false) &&
		!megasas_sync_pd_seq_num(instance, true))
		instance->use_seqnum_jbod_fp = 1;
	else
		instance->use_seqnum_jbod_fp = 0;
}

/**
 * megasas_init_fw -	Initializes the FW
 * @instance:		Adapter soft state
 *
 * This is the main function for initializing firmware
 */

static int megasas_init_fw(struct megasas_instance *instance)
{
	u32 max_sectors_1;
	u32 max_sectors_2;
	u32 tmp_sectors, msix_enable, scratch_pad_2, scratch_pad_3;
	struct megasas_register_set __iomem *reg_set;
	unsigned long bar_list;
	struct megasas_ctrl_info *ctrl_info = NULL;
	int i, j, loop, fw_msix_count = 0;
	struct IOV_111 *iovPtr;
	struct fusion_context *fusion;
	
	fusion = instance->ctrl_context;

	/* Find first memory bar */
        bar_list = pci_select_bars(instance->pdev, IORESOURCE_MEM);
        instance->bar = find_first_bit(&bar_list, sizeof(unsigned long));
        instance->base_addr = pci_resource_start(instance->pdev, instance->bar);
        if (pci_request_selected_regions(instance->pdev, instance->bar, "megasas: LSI")) {
		printk(KERN_DEBUG "megasas: IO memory region busy!\n");
		return -EBUSY;
        }

	instance->reg_set = ioremap_nocache(instance->base_addr, 8192);

	if (!instance->reg_set) {
		printk(KERN_DEBUG "megasas: Failed to map IO mem\n");
		goto fail_ioremap;
	}

	reg_set = instance->reg_set;
	
	if (fusion)
		instance->instancet = &megasas_instance_template_fusion;
	else {
 		switch(MR_PCI_ID(instance->pdev))
		{
 			case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1078R):
 			case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1078DE):
				instance->instancet = &megasas_instance_template_ppc;
				break;
 			case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1078GEN2):
 			case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0079GEN2):
				instance->instancet = &megasas_instance_template_gen2;
				break;
 			case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0073SKINNY):
 			case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0071SKINNY):
				instance->instancet = &megasas_instance_template_skinny;
				break;
 			case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1064R):
 			case MR_PCI_ID_DELL(PCI_DEVICE_ID_DELL_PERC5):
			default:
				instance->instancet = &megasas_instance_template_xscale;
				instance->pd_list_not_supported = 1;
				break;
		}
	}

	if (megasas_transition_to_ready(instance, 0)) {
		atomic_set(&instance->fw_reset_no_pci_access, 1);
		instance->instancet->adp_reset
			(instance, instance->reg_set);
		atomic_set(&instance->fw_reset_no_pci_access, 0);
		printk(KERN_INFO
			"megasas: FW restarted successfully from Kdump..!\n");

		/*waitting for about 30 second before retry*/
		ssleep(30);

		if (megasas_transition_to_ready(instance, 0))
			goto fail_ready_state;
	}

	if (instance->is_ventura) {
		scratch_pad_3 = readl(&instance->reg_set->outbound_scratch_pad_3);
#if VD_EXT_DEBUG 
		dev_info(&instance->pdev->dev, "scratch_pad3 0x%x\n", scratch_pad_3);
#endif
		instance->maxRaidMapSize = ((scratch_pad_3 >>
						MR_MAX_RAID_MAP_SIZE_OFFSET_SHIFT) & 
						MR_MAX_RAID_MAP_SIZE_MASK);
	}

	/* Check if MSI-X is supported while in ready state */
	msix_enable = (instance->instancet->read_fw_status_reg(reg_set) & 0x4000000) >> 0x1a;
	if (msix_enable && !msix_disable) {
		scratch_pad_2 = readl
			(&instance->reg_set->outbound_scratch_pad_2);
		/* Check max MSI-X vectors */
		if (fusion) {
				if (fusion->adapter_type == THUNDERBOLT_SERIES) { /* Thunderbolt family*/
					instance->msix_vectors = (scratch_pad_2
						& MR_MAX_REPLY_QUEUES_OFFSET) + 1;
					fw_msix_count = instance->msix_vectors;
					if (msix_vectors)
						instance->msix_vectors = min(msix_vectors,
							 instance->msix_vectors);
				} else { /* Invader series supports more than 8 MSI-x vectors*/ 
					instance->msix_vectors = ((scratch_pad_2
						& MR_MAX_REPLY_QUEUES_EXT_OFFSET)
						>> MR_MAX_REPLY_QUEUES_EXT_OFFSET_SHIFT) + 1;
					if (instance->msix_vectors > 16 ) {
						instance->msix_combined = true;
					}
					if (rdpq_enable)
						instance->is_rdpq = (scratch_pad_2 & MR_RDPQ_MODE_OFFSET) ?
									1 : 0;

					fw_msix_count = instance->msix_vectors;
					/* Save 1-15 reply post index address to local memory 
					* Index 0 is already saved from reg offset 
					* MPI2_REPLY_POST_HOST_INDEX_OFFSET
					*/
					for (loop = 1; loop < MR_MAX_MSIX_REG_ARRAY; loop++) {
						instance->reply_post_host_index_addr[loop] =
							(u32 *)((u8 *)instance->reg_set +
							MPI2_SUP_REPLY_POST_HOST_INDEX_OFFSET
							+ (loop * 0x10));
					}
					if (msix_vectors)
						instance->msix_vectors = min(msix_vectors,
							 instance->msix_vectors);
				}


		 } else /* MFI adapters */
			instance->msix_vectors = 1;

		/* 
 		 * Disable SMP affinity if number of logical CPUs exceeds number of 
 		 * MSI-x vectors supported
 		 */		
		if(instance->msix_vectors < num_online_cpus())
			smp_affinity_enable = 0;

		/* Don't bother allocating more MSI-X vectors than cpus */
		instance->msix_vectors = min(instance->msix_vectors,
					     (unsigned int)num_online_cpus());

		for (i = 0; i < instance->msix_vectors; i++)
			instance->msixentry[i].entry = i;
		if ((i = pci_enable_msix(instance->pdev, instance->msixentry, instance->msix_vectors)) >= 0) {
			if (i) {
				if (!pci_enable_msix(instance->pdev, instance->msixentry, i))
					instance->msix_vectors = i;
				else
					instance->msix_vectors = 0;
			}
		} else
			instance->msix_vectors = 0;
	}
	/*
	 * MSI-X host index 0 is common for all adapter.
	 * It is used for all MPT based Adapters.
	 */
	if (instance->msix_combined) {
		instance->reply_post_host_index_addr[0] =
				(u32 *)((u8 *)instance->reg_set +
				MPI2_SUP_REPLY_POST_HOST_INDEX_OFFSET);
	} else {
		instance->reply_post_host_index_addr[0] =
			(u32 *)((u8 *)instance->reg_set +
			MPI2_REPLY_POST_HOST_INDEX_OFFSET);
	}

	dev_info(&instance->pdev->dev,
		"firmware supports msix\t: (%d)", fw_msix_count);
	dev_info(&instance->pdev->dev,
		"current msix/online cpus\t: (%d/%d)\n",
		instance->msix_vectors, (unsigned int)num_online_cpus());
	dev_info(&instance->pdev->dev,
		"RDPQ mode\t: (%s)\n", instance->is_rdpq ? "enabled" : "disabled");

	tasklet_init(&instance->isr_tasklet, instance->instancet->tasklet,
		     (unsigned long)instance);

	if (megasas_setup_irqs(instance, 1))
			goto fail_setup_irqs;

	instance->ctrl_info = kzalloc(sizeof(struct megasas_ctrl_info), GFP_KERNEL);
	if ( instance->ctrl_info == NULL)
		goto fail_init_adapter;

	/*Below are defaul value for legacy Firmware. non-fusion based controllers*/
	instance->fw_supported_vd_count = MAX_LOGICAL_DRIVES;
	instance->fw_supported_pd_count = MAX_PHYSICAL_DEVICES;
	/* Do not send too many DCMD from init_adapter(), because interrupt
	 * is still disabled, so DCMD will be send using polling method.
	 * Current limit is - MEGASAS_FUSION_INTERNAL_CMDS +
	 * MEGASAS_FUSION_IOCTL_CMDS.
	 */
	if (instance->instancet->init_adapter(instance))
		goto fail_init_adapter;
	
	instance->instancet->enable_intr(instance);

	megasas_setup_jbod_map(instance);

	memset(instance->pd_list, 0, MEGASAS_MAX_PD * sizeof(struct megasas_pd_list));
 	if (megasas_get_pd_list(instance) < 0)
 		goto fail_get_ld_pd_list;

  	memset(instance->ld_ids, 0xff, MEGASAS_MAX_LD_IDS);
	//stream detection initialization
	if (instance->is_ventura) {
		fusion->streamDetectByLD = kmalloc(sizeof(PTR_LD_STREAM_DETECT) * MAX_LOGICAL_DRIVES_EXT,
						GFP_KERNEL );
		if(!fusion->streamDetectByLD) {
			dev_err(&instance->pdev->dev,
					"unable to allocate stream detection for pool of LDs\n");
			goto fail_get_ld_pd_list;
		}
		for(i = 0; i < MAX_LOGICAL_DRIVES_EXT; ++i) {
		 	fusion->streamDetectByLD[i] = kmalloc(sizeof(LD_STREAM_DETECT), GFP_KERNEL);
			if(!fusion->streamDetectByLD[i]) {
				dev_err(&instance->pdev->dev,
					"unable to allocate stream detect by LD\n ");
				 for (j = 0; j < i; ++j)
					kfree(fusion->streamDetectByLD[j]);
				kfree(fusion->streamDetectByLD);
				fusion->streamDetectByLD = NULL;
				goto fail_get_ld_pd_list;
			}
			memset(fusion->streamDetectByLD[i], 0, sizeof(LD_STREAM_DETECT));
			fusion->streamDetectByLD[i]->mruBitMap = MR_STREAM_BITMAP;
		}
	}
 	
 	if (megasas_ld_list_query(instance,
 				  MR_LD_QUERY_TYPE_EXPOSED_TO_HOST))
 		goto fail_get_ld_pd_list;
	
	/*
	 * Compute the max allowed sectors per IO: The controller info has two
	 * limits on max sectors. Driver should use the minimum of these two.
	 *
	 * 1 << stripe_sz_ops.min = max sectors per strip
	 *
	 * Note that older firmwares ( < FW ver 30) didn't report information
	 * to calculate max_sectors_1. So the number ended up as zero always.
	 */
	tmp_sectors = 0;
	ctrl_info = instance->ctrl_info;

	max_sectors_1 = (1 << ctrl_info->stripe_sz_ops.min) *
		le16_to_cpu(ctrl_info->max_strips_per_io);
	max_sectors_2 = le32_to_cpu(ctrl_info->max_request_size);

	tmp_sectors = min_t(u32, max_sectors_1 , max_sectors_2);
	

	instance->UnevenSpanSupport =
		ctrl_info->adapterOperations2.supportUnevenSpans;
	instance->peerIsPresent = ctrl_info->cluster.peerIsPresent;
	instance->passive = ctrl_info->cluster.passive;
	memcpy(instance->clusterId, ctrl_info->clusterId, sizeof(instance->clusterId));
	if(instance->UnevenSpanSupport) {
		struct fusion_context *fusion = instance->ctrl_context;
		if (MR_ValidateMapInfo(instance))
			fusion->fast_path_io = 1;
		else
			fusion->fast_path_io = 0;
	
	}
	if (ctrl_info->host_interface.SRIOV) {
		if (!ctrl_info->adapterOperations2.activePassive)
			instance->PlasmaFW111 = 1;

		if (!instance->PlasmaFW111)
			instance->requestorId =
				ctrl_info->iov.requestorId;
		else {
			iovPtr = (struct IOV_111 *)((unsigned char *)ctrl_info + IOV_111_OFFSET);
			instance->requestorId = iovPtr->requestorId;
		}
		printk(KERN_WARNING "megaraid_sas: I am VF "
		       "requestorId %d\n", instance->requestorId);
	}
	
	/* Check whether FW supports crash dump */
	instance->crash_dump_fw_support = ctrl_info->adapterOperations3.supportCrashDump;
	instance->crash_dump_drv_support = (instance->crash_dump_fw_support &&
				instance->crash_dump_buf);
	if (instance->crash_dump_drv_support)
		megasas_set_crash_dump_params(instance, MR_CRASH_BUF_TURN_OFF);
	else {
		if (instance->crash_dump_buf)
			pci_free_consistent(instance->pdev, CRASH_DMA_BUF_SIZE,
				instance->crash_dump_buf,
				instance->crash_dump_h);
		instance->crash_dump_buf = NULL;
	}
	
	dev_info(&instance->pdev->dev,
		"pci id\t\t: (0x%04x)/(0x%04x)/(0x%04x)/(0x%04x)\n",
		le16_to_cpu(ctrl_info->pci.vendor_id),
		le16_to_cpu(ctrl_info->pci.device_id),
		le16_to_cpu(ctrl_info->pci.sub_vendor_id),
		le16_to_cpu(ctrl_info->pci.sub_device_id));
	dev_info(&instance->pdev->dev, "unevenspan support	: %s\n",
		instance->UnevenSpanSupport ? "yes" : "no");
	dev_info(&instance->pdev->dev, "firmware crash dump	: %s\n",
		instance->crash_dump_drv_support ? "yes" : "no");
	dev_info(&instance->pdev->dev, "jbod sync map		: %s\n",
		instance->use_seqnum_jbod_fp ? "yes" : "no");

	instance->max_sectors_per_req = instance->max_num_sge *
						SGE_BUFFER_SIZE / 512;
	if (tmp_sectors && (instance->max_sectors_per_req > tmp_sectors))
		instance->max_sectors_per_req = tmp_sectors;
	

	/* Check for valid throttlequeuedepth module parameter */
	if (throttlequeuedepth && 
			throttlequeuedepth <= instance->max_scsi_cmds)
		instance->throttlequeuedepth = throttlequeuedepth;
	else
		instance->throttlequeuedepth =
				MEGASAS_THROTTLE_QUEUE_DEPTH;
	
	if (resetwaittime > MEGASAS_RESET_WAIT_TIME)
		resetwaittime = MEGASAS_RESET_WAIT_TIME;
	
	if ((scmd_timeout < 10) || (scmd_timeout > MEGASAS_DEFAULT_CMD_TIMEOUT))
		scmd_timeout = MEGASAS_DEFAULT_CMD_TIMEOUT;

	/* Launch SR-IOV heartbeat timer */
	if (instance->requestorId) {
		if (!megasas_sriov_start_heartbeat(instance, 1))
			megasas_start_timer(instance,
					    &instance->sriov_heartbeat_timer,
					    megasas_sriov_heartbeat_handler,
					    MEGASAS_SRIOV_HEARTBEAT_INTERVAL_VF);
		else {
			instance->skip_heartbeat_timer_del = 1;
			goto fail_init_adapter;
		}
	}
	return 0;

fail_get_ld_pd_list:
	instance->instancet->disable_intr(instance);
fail_init_adapter:
	if (instance->msix_vectors)
		for (i = 0; i < instance->msix_vectors; i++) {
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 6)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
			if (smp_affinity_enable)
				irq_set_affinity_hint(
					instance->msixentry[i].vector, NULL);
#endif
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
		}
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
fail_setup_irqs:
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
	instance->msix_vectors = 0;
fail_ready_state:
	if (instance->ctrl_info) {
		kfree(instance->ctrl_info);
		instance->ctrl_info = NULL;
	}
	iounmap(instance->reg_set);
fail_ioremap:
	pci_release_selected_regions(instance->pdev, instance->bar);

	dev_err(&instance->pdev->dev, "Failed from %s %d\n",
			__func__, __LINE__);
	return -EINVAL;
}

/**
 * megasas_release_mfi -	Reverses the FW initialization
 * @intance:			Adapter soft state
 */
static void megasas_release_mfi(struct megasas_instance *instance)
{
	u32 reply_q_sz = sizeof(u32) * (instance->max_mfi_cmds + 1);

	if (instance->reply_queue)
		pci_free_consistent(instance->pdev, reply_q_sz,
			    instance->reply_queue, instance->reply_queue_h);

	megasas_free_cmds(instance);

	iounmap(instance->reg_set);

	megasas_remove_cpx( instance );

	pci_release_selected_regions(instance->pdev, instance->bar);
}

/**
 * megasas_get_seq_num -	Gets latest event sequence numbers
 * @instance:			Adapter soft state
 * @eli:			FW event log sequence numbers information
 *
 * FW maintains a log of all events in a non-volatile area. Upper layers would
 * usually find out the latest sequence number of the events, the seq number at
 * the boot etc. They would "read" all the events below the latest seq number
 * by issuing a direct fw cmd (DCMD). For the future events (beyond latest seq
 * number), they would subsribe to AEN (asynchronous event notification) and
 * wait for the events to happen.
 */
static int
megasas_get_seq_num(struct megasas_instance *instance,
		    struct megasas_evt_log_info *eli)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	struct megasas_evt_log_info *el_info;
	dma_addr_t el_info_h = 0;
	int ret = 0;

	cmd = megasas_get_cmd(instance);

	if (!cmd) {
		return -ENOMEM;
	}

	dcmd = &cmd->frame->dcmd;
	el_info = pci_alloc_consistent(instance->pdev,
				       sizeof(struct megasas_evt_log_info),
				       &el_info_h);

	if (!el_info) {
		megasas_return_cmd(instance, cmd);
		return -ENOMEM;
	}

	memset(el_info, 0, sizeof(*el_info));
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = cpu_to_le32(sizeof(struct megasas_evt_log_info));
	dcmd->opcode = cpu_to_le32(MR_DCMD_CTRL_EVENT_GET_INFO);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(el_info_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(sizeof(struct megasas_evt_log_info));

	ret = megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS);
	if (ret != DCMD_SUCCESS) {
		dev_err(&instance->pdev->dev, "Failed from %s %d\n",
			__func__, __LINE__);
		goto dcmd_failed;
	}

	eli->newest_seq_num = le32_to_cpu(el_info->newest_seq_num);
	eli->oldest_seq_num = le32_to_cpu(el_info->oldest_seq_num);
	eli->clear_seq_num = le32_to_cpu(el_info->clear_seq_num);
	eli->shutdown_seq_num = le32_to_cpu(el_info->shutdown_seq_num);
	eli->boot_seq_num = le32_to_cpu(el_info->boot_seq_num);


dcmd_failed:
	pci_free_consistent(instance->pdev, sizeof(struct megasas_evt_log_info),
			    el_info, el_info_h);
	
	megasas_return_cmd(instance, cmd);

	return ret;
}

/**
 * megasas_register_aen -	Registers for asynchronous event notification
 * @instance:			Adapter soft state
 * @seq_num:			The starting sequence number
 * @class_locale:		Class of the event
 *
 * This function subscribes for AEN for events beyond the @seq_num. It requests
 * to be notified if and only if the event is of type @class_locale
 */
static int
megasas_register_aen(struct megasas_instance *instance, u32 seq_num,
		     u32 class_locale_word)
{
	int ret_val;
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;
	union megasas_evt_class_locale curr_aen;
	union megasas_evt_class_locale prev_aen;

	/*
	 * If there an AEN pending already (aen_cmd), check if the
	 * class_locale of that pending AEN is inclusive of the new
	 * AEN request we currently have. If it is, then we don't have
	 * to do anything. In other words, whichever events the current
	 * AEN request is subscribing to, have already been subscribed
	 * to.
	 *
	 * If the old_cmd is _not_ inclusive, then we have to abort
	 * that command, form a class_locale that is superset of both
	 * old and current and re-issue to the FW
	 */

	curr_aen.word = class_locale_word;

	if (instance->aen_cmd) {

		prev_aen.word = instance->aen_cmd->frame->dcmd.mbox.w[1];
		prev_aen.members.locale = le16_to_cpu(prev_aen.members.locale);

		/*
		 * A class whose enum value is smaller is inclusive of all
		 * higher values. If a PROGRESS (= -1) was previously
		 * registered, then a new registration requests for higher
		 * classes need not be sent to FW. They are automatically
		 * included.
		 *
		 * Locale numbers don't have such hierarchy. They are bitmap
		 * values
		 */
		if ((prev_aen.members.class <= curr_aen.members.class) &&
		    !((prev_aen.members.locale & curr_aen.members.locale) ^
		      curr_aen.members.locale)) {
			/*
			 * Previously issued event registration includes
			 * current request. Nothing to do.
			 */
			printk(KERN_INFO "%s[%d]: already registered\n",
				__FUNCTION__, instance->host->host_no);
			return 0;
		} else {
			curr_aen.members.locale |= prev_aen.members.locale;

			if (prev_aen.members.class < curr_aen.members.class)
				curr_aen.members.class = prev_aen.members.class;

			instance->aen_cmd->abort_aen = 1;
			ret_val = megasas_issue_blocked_abort_cmd(instance,
								  instance->
								  aen_cmd, 0);

			if (ret_val) {
				printk(KERN_DEBUG "megasas: Failed to abort "
				       "previous AEN command\n");
				return ret_val;
			}
		}
	}

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return -ENOMEM;

	dcmd = &cmd->frame->dcmd;

	memset(instance->evt_detail, 0, sizeof(struct megasas_evt_detail));

	/*
	 * Prepare DCMD for aen registration
	 */
	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 1;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_READ);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = cpu_to_le32(sizeof(struct megasas_evt_detail));
	dcmd->opcode = cpu_to_le32(MR_DCMD_CTRL_EVENT_WAIT);
	dcmd->mbox.w[0] = cpu_to_le32(seq_num);
        instance->last_seq_num = seq_num;
	dcmd->mbox.w[1] = cpu_to_le32(curr_aen.word);
	dcmd->sgl.sge32[0].phys_addr = cpu_to_le32(instance->evt_detail_h);
	dcmd->sgl.sge32[0].length = cpu_to_le32(sizeof(struct megasas_evt_detail));

	if (instance->aen_cmd != NULL) {
		megasas_return_cmd(instance, cmd);
		return 0;
	}

	/*
	 * Store reference to the cmd used to register for AEN. When an
	 * application wants us to register for AEN, we have to abort this
	 * cmd and re-register with a new EVENT LOCALE supplied by that app
	 */
	instance->aen_cmd = cmd;

	/*
	 * Issue the aen registration frame
	 */
	return (instance->instancet->issue_dcmd(instance, cmd));

}

/**
 * megasas_start_aen -	Subscribes to AEN during driver load time
 * @instance:		Adapter soft state
 */
static int megasas_start_aen(struct megasas_instance *instance)
{
	struct megasas_evt_log_info eli;
	union megasas_evt_class_locale class_locale;

	/*
	 * Get the latest sequence number from FW
	 */
	memset(&eli, 0, sizeof(eli));

	if (megasas_get_seq_num(instance, &eli))
		return -1;

	/*
	 * Register AEN with FW for latest sequence number plus 1
	 */
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;

	return megasas_register_aen(instance,
			eli.newest_seq_num + 1,
			class_locale.word);
}

#if (defined(CONFIG_SUSE_KERNEL) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))
static ssize_t
sysfs_max_sectors_read(struct kobject *kobj,
		       struct bin_attribute *bin_attr,
                       char *buf, loff_t off, size_t count)
#else
static ssize_t
sysfs_max_sectors_read(struct file *filep, struct kobject *kobj,
		       struct bin_attribute *bin_attr,
                       char *buf, loff_t off, size_t count)
#endif
{
       struct device *dev = container_of(kobj, struct device, kobj);

       struct Scsi_Host *host = class_to_shost(dev);

       struct megasas_instance *instance =
                               (struct megasas_instance *)host->hostdata;

       count = sprintf(buf,"%u\n", instance->max_sectors_per_req);

       return count+1;
}

static struct bin_attribute sysfs_max_sectors_attr = {
       .attr = {
               .name = "max_sectors",
               .mode = S_IRUSR|S_IRGRP|S_IROTH,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))
               .owner = THIS_MODULE,
#endif
       },
       .size = 7,
       .read = sysfs_max_sectors_read,
};

/**
 * megasas_io_attach -	Attaches this driver to SCSI mid-layer
 * @instance:		Adapter soft state
 */
static int megasas_io_attach(struct megasas_instance *instance)
{
	struct Scsi_Host *host = instance->host;
	u32		error;

	/*
	 * Export parameters required by SCSI mid-layer
	 */
	host->irq = instance->pdev->irq;
	host->unique_id = instance->unique_id;
	host->can_queue = instance->max_scsi_cmds;
	host->this_id = instance->init_id;
	host->sg_tablesize = instance->max_num_sge;

	if (instance->fw_support_ieee)
		instance->max_sectors_per_req = MEGASAS_MAX_SECTORS_IEEE;

       /*
        * Check if the module parameter value for max_sectors can be used
        */
       if (max_sectors && max_sectors < instance->max_sectors_per_req)
               instance->max_sectors_per_req = max_sectors;
       else {
               if (max_sectors) {
                        if (((MR_PCI_ID(instance->pdev) ==
                                MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS1078GEN2)) ||
                                (MR_PCI_ID(instance->pdev) ==
                                MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0079GEN2))) &&
                               (max_sectors <= MEGASAS_MAX_SECTORS)) {
                                       instance->max_sectors_per_req = max_sectors;
                       } else {
                               printk(KERN_INFO "megasas: max_sectors should be > 0 and"
                                       "<= %d (or < 1MB for GEN2 controller)\n",
                                       instance->max_sectors_per_req);
                       }
               }
       }

	host->max_sectors = instance->max_sectors_per_req;
	host->cmd_per_lun = MEGASAS_DEFAULT_CMD_PER_LUN;
	host->max_channel = MEGASAS_MAX_CHANNELS - 1;
	host->max_id = MEGASAS_MAX_DEV_PER_CHANNEL;
	host->max_lun = MEGASAS_MAX_LUN;
	host->max_cmd_len = 16;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0))
	error = scsi_init_shared_tag_map(host, host->can_queue);
	if (error) {
		dev_err(&instance->pdev->dev,
			"Failed to shared tag from %s %d\n",
			__func__, __LINE__);
		return -ENODEV;
	}
#endif
	/*
	 * Notify the mid-layer about the new controller
	 */
	if (scsi_add_host(host, &instance->pdev->dev)) {
		dev_err(&instance->pdev->dev,
			"Failed to add host from %s %d\n",
			__func__, __LINE__);
		return -ENODEV;
	}

        /*                                                                      
        * Create sysfs entries for module paramaters                            
        */
       error = sysfs_create_bin_file(&instance->host->shost_dev.kobj,
                       &sysfs_max_sectors_attr);

       if (error) {
               printk(KERN_INFO "megasas: Error in creating the sysfs entry"
                               " max_sectors.\n");
               goto out_remove_host;
       }

	return 0;

out_remove_host:
        scsi_remove_host(host);
        return error;
}

static int
megasas_set_dma_mask(struct pci_dev *pdev)
{
	/*
	 * All our contollers are capable of performing 64-bit DMA
	 */
	if (IS_DMA64) {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) != 0) {

			if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)
				goto fail_set_dma_mask;
		}
	} else {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0)
			goto fail_set_dma_mask;
	}
	/*
	 * Ensure that all data structures are allocated in 32-bit
	 * memory.
	 */
	if (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)) != 0) {
		/* Try 32bit DMA mask and 32 bit Consistent dma mask */
		if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))
			&& !pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)))
			printk("megasas: set 32bit DMA mask and 32 bit consistent mask\n");
		else
			goto fail_set_dma_mask;
	}

	return 0;

fail_set_dma_mask:
	return 1;
}

/**
 * megasas_setup_irqs -	register interrupt with IRQ sub system.
 * @instance:				Adapter soft state
 * @is_probe:				Driver probe check
 *
 * Do not enable interrupt, only setup ISRs.
 *
 * Return 0 on success.
 */
static int
megasas_setup_irqs(struct megasas_instance *instance, u8 is_probe)
{
	int i, j, cpu;
	struct pci_dev *pdev;

	pdev = instance->pdev;

try_io_apic:
	if (!instance->msix_vectors) {
		instance->irq_context[0].instance = instance;
		instance->irq_context[0].MSIxIndex = 0;
		if (request_irq(pdev->irq, instance->instancet->service_isr,
			IRQF_SHARED, "megasas", &instance->irq_context[0])) {
			dev_err(&instance->pdev->dev,
					"Failed to register IRQ from %s %d\n",
					__func__, __LINE__);
			return -1;
		}
		return 0;
	}
	/* Try MSI-x */
	cpu = cpumask_first(cpu_online_mask);
	for (i = 0; i < instance->msix_vectors; i++) {
		instance->irq_context[i].instance = instance;
		instance->irq_context[i].MSIxIndex = i;
		if (request_irq(instance->msixentry[i].vector,
			instance->instancet->service_isr, 0, "megasas",
			&instance->irq_context[i])) {
			dev_err(&instance->pdev->dev,
				"Failed to register IRQ for vector %d.\n", i);
			for (j = 0; j < i; j++) {
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 6)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
				if (smp_affinity_enable)
					irq_set_affinity_hint(
						instance->msixentry[j].vector, NULL);
#endif
				free_irq(instance->msixentry[j].vector,
					&instance->irq_context[j]);
			}
			/* Retry irq register for IO_APIC*/
			instance->msix_vectors = 0;
			if (is_probe)
				goto try_io_apic;
			else
				return -1;
		}
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 6)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
		if (smp_affinity_enable) {
			if (irq_set_affinity_hint(instance->msixentry[i].vector,
				get_cpu_mask(cpu)))
				dev_err(&instance->pdev->dev,
					"Failed to set affinity hint"
					" for cpu %d\n", cpu);
			cpu = cpumask_next(cpu, cpu_online_mask);
		}
#endif
	}
	return 0;
}

/**
 * megasas_need_for_drive_ordering - Identify if the card needs drive ordering
 * @instance:		Adapter structure
 * 
 * This function looks at the Device ID & SubSystem Vendor ID and decides 
 * whether the drive ordering needs to be performed or not
 *
 * returns: 1 - Needs Ordering
 *          0 - Does Not Need Ordering
 */

static u8
megasas_need_for_drive_ordering(struct megasas_instance *instance)
{
	u8 ret;

 	if (((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_INVADER)) ||
 	    (MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_FURY))) &&
	    (instance->pdev->subsystem_vendor == PCI_VENDOR_ID_DELL))
		ret = 1; /* Needs ordering */
	else
		ret = 0; /* Does NOT need ordering */

	return ret;
}

/**
 * megasas_expose_drives_to_os - expose drive to OS by scanning or adding 
 * @instance:		Adapter structure
 * 
 * This function either invokes scsi_scan_host() to add drives for 
 * cards which doesn't need drive ordering or invokes scsi_add_device()
 * which needs drive ordering. 
 */

static void
megasas_expose_drives_to_os(struct megasas_instance *instance)
{
	int i, j, ret = 0;
	struct Scsi_Host *host;
	struct megasas_drive_order *drv_odr;
	u16 pd_index = 0;
	u16 ld_index = 0;

	host = instance->host;
	drv_odr = &instance->drive_order;

	drv_odr->needs_ordering = 
	    megasas_need_for_drive_ordering(instance);

	/* Check if drive ordering is needed, if not 
	 * invoke SCSI layer to scan */
	if (!drv_odr->needs_ordering) {
		/* Trigger SCSI to scan our drives */
		scsi_scan_host(host);
		return;
	}

	/* Drive ordering is needed, obtain details on boot drive */
	ret = megasas_get_bios_data(instance);
        if (ret) {
		dev_info(&instance->pdev->dev, "%s : Failure in updating "
		    "boot disk data-structure disk_order\n", __func__);
	}

	/* Check if boot drive is behind this controller, if so 
	 * add that first */
	if (drv_odr->boot_fw_tgt_id != MEGASAS_INVALID_TARGET_ID) {
		ret = scsi_add_device(host, drv_odr->boot_drive_channel_id,
		    drv_odr->boot_drive_tgt_id, 0);
		if (ret) {
			dev_info(&instance->pdev->dev, "%s : Failure in "
			    "scsi_add_device() for boot drive (%d)\n",
			    __func__, ret);
		}
	}

	/* Case 1: Boot drive is not there
	 * Case 2: Boot drive is there & it is LD 
	 * (which is already exposed above) */ 
	if ((drv_odr->boot_fw_tgt_id == MEGASAS_INVALID_TARGET_ID) || 
	    ((drv_odr->boot_fw_tgt_id != MEGASAS_INVALID_TARGET_ID) && 
	    (!drv_odr->boot_drive_is_pd))) {
 
		/* Expose LDs */
		for (i=0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				ld_index = (i * MEGASAS_MAX_DEV_PER_CHANNEL)
				    + j;

				/* Check if LD is present and it is not 
				 * the already added boot drive */
				if ((instance->ld_ids[ld_index] != 0xff) &&
				    (ld_index != drv_odr->boot_fw_tgt_id)) {
					ret = scsi_add_device(host, 
					    MEGASAS_MAX_PD_CHANNELS + i, j, 0);
					if (ret) {
						dev_info(&instance->pdev->dev,
						    "%s : Failure (%d) in "
						    "scsi_add_device() for LD "
						    "Channel:%d, Target:%d\n",
						    __func__, ret, 
						    (MEGASAS_MAX_PD_CHANNELS 
						    + i), j);
					}
				}
			}
		}

		/* Expose System PDs */
		for (i=0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				pd_index = (i * MEGASAS_MAX_DEV_PER_CHANNEL)
				    + j;
				if (instance->pd_list[pd_index].driveState == 
				    MR_PD_STATE_SYSTEM) {
					ret = scsi_add_device(host, i, j, 0);
					if (ret) {
						dev_info(&instance->pdev->dev,
						    "%s : Failure (%d) in "
						    "scsi_add_device() for "
						    "System PD Channel: %d, "
						    "Target: %d\n",
						    __func__, ret, i, j);
					}
				}
			}
		}
		return;
	}

	/* Case 3: Boot drive is there & it is System PD 
	 * (which is already exposed above) */
	if ((drv_odr->boot_fw_tgt_id != MEGASAS_INVALID_TARGET_ID) && 
	    (drv_odr->boot_drive_is_pd)) {

		/* Expose System PDs */
		for (i=0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				pd_index = (i * MEGASAS_MAX_DEV_PER_CHANNEL)
				    + j;

				/* Check if System PD is present and 
				 * it is not the already added boot drive */
				if ((instance->pd_list[pd_index].driveState ==
				    MR_PD_STATE_SYSTEM) &&
				    (pd_index != drv_odr->boot_fw_tgt_id)) { 
					ret = scsi_add_device(host, i, j, 0);
					if (ret) {
						dev_info(&instance->pdev->dev, 
						    "%s : Failure (%d) in "
						    "scsi_add_device() for "
						    "System PD Channel: %d, "
						    "Target: %d\n",
						    __func__, ret, i, j);
					}
				}
			}
		}

		/* Expose LDs */
		for (i=0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				ld_index = (i * MEGASAS_MAX_DEV_PER_CHANNEL)
				    + j;

				/* Check if LD is present */
				if (instance->ld_ids[ld_index] != 0xff) {
					ret = scsi_add_device(host, 
					    MEGASAS_MAX_PD_CHANNELS + i, j, 0);
					if (ret) {
						dev_info(&instance->pdev->dev,
						    "%s : Failure (%d) in "
						    "scsi_add_device() for LD "
						    "Channel:%d, Target:%d\n",
						    __func__, ret, 
						    (MEGASAS_MAX_PD_CHANNELS 
						    + i), j);
					}
				}
			}
		}
		return;
	}
}

/**
 * megasas_probe_one -	PCI hotplug entry point
 * @pdev:		PCI device structure
 * @id:			PCI ids of supported hotplugged adapter	
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,10))
static int __devinit
megasas_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
#else
static int
megasas_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
#endif
{
	int rval, pos, i;
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	u16 control = 0;
	struct fusion_context *fusion = NULL;

	/* Reset MSI-X in the kdump kernel */
	if (reset_devices) {
		pos = pci_find_capability(pdev, PCI_CAP_ID_MSIX);
		if (pos) {
			pci_read_config_word(pdev, msi_control_reg(pos), &control);
			if (control & PCI_MSIX_FLAGS_ENABLE) {
				dev_info(&pdev->dev, "resetting MSI-X\n");
				pci_write_config_word(pdev, msi_control_reg(pos), control & ~PCI_MSIX_FLAGS_ENABLE);
			}
		}
	}

	/*
	 * PCI prepping: enable device set bus mastering and dma mask
	 */
	rval = pci_enable_device(pdev);

	if (rval) {
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;

#ifdef RHEL_MAJOR 
#if (RHEL_MAJOR == 6 && RHEL_MINOR >= 2)
	megasas_template.lockless = 1;
#endif
#endif

	host = scsi_host_alloc(&megasas_template,
			       sizeof(struct megasas_instance));

	if (!host) {
		printk(KERN_DEBUG "megasas: scsi_host_alloc failed\n");
		goto fail_alloc_instance;
	}

	instance = (struct megasas_instance *)host->hostdata;
	memset(instance, 0, sizeof(*instance));
	atomic_set( &instance->fw_reset_no_pci_access, 0 );

	/*
	 * Initialize PCI related and misc parameters
	 */
	instance->pdev = pdev;
	instance->host = host;
	instance->unique_id = pdev->bus->number << 8 | pdev->devfn;
	instance->init_id = MEGASAS_DEFAULT_INIT_ID;
	instance->ctrl_info = NULL;

 	switch(MR_PCI_ID(instance->pdev))
	{
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_VENTURA):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_HARPOON):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_TOMCAT):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_VENTURA_4PORT):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_CRUSADER_4PORT):
			instance->is_ventura = true;

 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_FUSION):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_PLASMA):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_INVADER):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_INTRUDER):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_INTRUDER_24):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_CUTLASS_52):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_CUTLASS_53):
 		case MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_FURY):
		{
			if (megasas_alloc_fusion_context(instance)) {
				megasas_free_fusion_context(instance);
				goto fail_alloc_dma_buf;
			}
			fusion = instance->ctrl_context;

 			if ((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_FUSION)) ||
 				(MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_PLASMA)))
				fusion->adapter_type = THUNDERBOLT_SERIES;
			else if (instance->is_ventura)
				fusion->adapter_type = VENTURA_SERIES;
			else
				fusion->adapter_type = INVADER_SERIES;

		}
			break;
		default: /* For all other supported controllers */

			instance->producer = pci_alloc_consistent(pdev, sizeof(u32),
									&instance->producer_h);
			instance->consumer = pci_alloc_consistent(pdev, sizeof(u32),
									&instance->consumer_h);

			if (!instance->producer || !instance->consumer) {
				printk(KERN_DEBUG "megasas: Failed to allocate memory for "
				"producer, consumer\n");
				goto fail_alloc_dma_buf;
			}

			*instance->producer = 0;
			*instance->consumer = 0;
			break;
 	}

    instance->verbuf = pci_alloc_consistent(pdev,
						MEGASAS_MAX_NAME*sizeof(u32),
                                                &instance->verbuf_h);
	if (!instance->verbuf) {
		printk(KERN_DEBUG "megasas: Can't allocate version buffer\n");
	}

	if (!reset_devices)
		instance->system_info_buf = pci_alloc_consistent(pdev,
						MR_DRV_SYSTEM_INFO_SIZE, &instance->system_info_h);
	
	if (!instance->system_info_buf) {
		printk(KERN_DEBUG "megasas: Can't allocate system info buffer\n");
	}
	
	/* Crash dump related initialisation*/
	instance->drv_buf_index = 0;
	instance->drv_buf_alloc = 0;
	instance->crash_dump_fw_support = 0; 	/*support crash dump feature*/
	instance->crash_dump_app_support = 0; 	/*application running to collect crash dump*/
	instance->fw_crash_state = UNAVAILABLE; /*crash dump state */
	instance->crash_dump_buf = NULL;
	spin_lock_init(&instance->crashdump_lock); /*Init spinlock used by sys interface for crash dump*/

	if (crashdump_enable)
		instance->crash_dump_buf = pci_alloc_consistent(pdev,
					CRASH_DMA_BUF_SIZE,
					&instance->crash_dump_h);

	if (!instance->crash_dump_buf)
		printk(KERN_DEBUG "megasas: Can't allocate crash dump buffer\n");
		
	instance->flag_ieee = 0;
	instance->ev = NULL;
 	instance->issuepend_done = 1;
	atomic_set(&instance->adprecovery, MEGASAS_HBA_OPERATIONAL);
	instance->is_imr = 0;
	 /* Setting boot_fw_tgt_id to none */
	instance->drive_order.boot_fw_tgt_id = MEGASAS_INVALID_TARGET_ID;
	megasas_poll_wait_aen = 0;

	instance->evt_detail = pci_alloc_consistent(pdev,
						    sizeof(struct
							   megasas_evt_detail),
						    &instance->evt_detail_h);

	if (!instance->evt_detail) {
		printk(KERN_DEBUG "megasas: Failed to allocate memory for "
		       "event detail structure\n");
		goto fail_alloc_dma_buf;
	}

	if (!reset_devices)
		instance->pd_info = pci_alloc_consistent(pdev,
			sizeof(struct MR_PD_INFO), &instance->pd_info_h);

	if (!instance->pd_info)
		dev_err(&instance->pdev->dev, "Failed to alloc mem for pd_info\n");

	/*
	 * Initialize locks and queues
	 */
	INIT_LIST_HEAD(&instance->cmd_pool);
	INIT_LIST_HEAD(&instance->internal_reset_pending_q);

	atomic_set(&instance->fw_outstanding,0);

	init_waitqueue_head(&instance->int_cmd_wait_q);
	init_waitqueue_head(&instance->abort_cmd_wait_q);

	spin_lock_init(&instance->cmd_pool_lock);
	spin_lock_init(&instance->hba_lock);
	spin_lock_init(&instance->stream_lock);

	spin_lock_init(&instance->completion_lock);

	mutex_init(&instance->hba_mutex);
	mutex_init(&instance->reset_mutex);

 	if ((MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0073SKINNY)) ||
		(MR_PCI_ID(instance->pdev) == MR_PCI_ID_LSI(PCI_DEVICE_ID_LSI_SAS0071SKINNY)))
		instance->flag_ieee = 1;
		
	megasas_dbg_lvl = 0;
	instance->flag = 0;
	instance->unload = 1;
	instance->last_time = 0;
	instance->disableOnlineCtrlReset = 1;
	instance->UnevenSpanSupport = 0;

	if (instance->ctrl_context) {
		INIT_WORK(&instance->work_init, megasas_fusion_ocr_wq);
		INIT_WORK(&instance->crash_init, megasas_fusion_crash_dump_wq);
	}
	else
		INIT_WORK(&instance->work_init, process_fw_state_change_wq);

	/*
	 * Initialize Firmware
	 */
	if (megasas_init_fw(instance))
		goto fail_init_mfi;

	if (instance->requestorId) {
		if (instance->PlasmaFW111) {
			instance->vf_affiliation_111 =
				pci_alloc_consistent(pdev, sizeof(struct MR_LD_VF_AFFILIATION_111),
						     &instance->vf_affiliation_111_h);
			if (!instance->vf_affiliation_111)
				printk(KERN_WARNING "megasas: Can't allocate "
				       "memory for VF affiliation buffer\n");
		} else {
			instance->vf_affiliation =
				pci_alloc_consistent(pdev,
						     (MAX_LOGICAL_DRIVES + 1) *
						     sizeof(struct MR_LD_VF_AFFILIATION),
						     &instance->vf_affiliation_h);
			if (!instance->vf_affiliation)
				printk(KERN_WARNING "megasas: Can't allocate "
				       "memory for VF affiliation buffer\n");
		}
	}

	/*
	 * Store instance in PCI softstate
	 */
	pci_set_drvdata(pdev, instance);

	/*
	 * Add this controller to megasas_mgmt_info structure so that it
	 * can be exported to management applications
	 */
	megasas_mgmt_info.count++;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = instance;
	megasas_mgmt_info.max_index++;

	/*
	 * Register with SCSI mid-layer
	 */
	if (megasas_io_attach(instance))
		goto fail_io_attach;
	

	/*
	 * Initiate AEN (Asynchronous Event Notification)
	 */
	if (megasas_start_aen(instance)) {
		printk(KERN_DEBUG "megasas: start aen failed\n");
		goto fail_start_aen;
	}
	
	instance->unload = 0;
	
	/* Expose the System PD/LD to Operating System */
	megasas_expose_drives_to_os(instance);


	/* Get current SR-IOV LD/VF affiliation */
	if (instance->requestorId)
		megasas_get_ld_vf_affiliation(instance, 1);
	
	return 0;

      fail_start_aen:
      fail_io_attach:
	megasas_mgmt_info.count--;
	megasas_mgmt_info.instance[megasas_mgmt_info.max_index] = NULL;
	megasas_mgmt_info.max_index--;

	pci_set_drvdata(pdev, NULL);
	instance->instancet->disable_intr(instance);
	if (instance->msix_vectors)
		for (i = 0; i < instance->msix_vectors; i++) {
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 6)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
			if (smp_affinity_enable)
				irq_set_affinity_hint(
					instance->msixentry[i].vector, NULL);
#endif
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
		}
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->ctrl_context)
		megasas_release_fusion(instance);
	else
		megasas_release_mfi(instance);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
      fail_init_mfi:
      fail_alloc_dma_buf:
	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				    instance->evt_detail,
				    instance->evt_detail_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				    instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				    instance->consumer_h);

	scsi_host_put(host);

      fail_alloc_instance:
      fail_set_dma_mask:
	pci_disable_device(pdev);

	return -ENODEV;
}

/**
 * megasas_flush_cache -	Requests FW to flush all its caches
 * @instance:			Adapter soft state
 */
static void megasas_flush_cache(struct megasas_instance *instance)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	if (atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR)
		return;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_NONE);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = cpu_to_le32(MR_DCMD_CTRL_CACHE_FLUSH);
	dcmd->mbox.b[0] = MR_FLUSH_CTRL_CACHE | MR_FLUSH_DISK_CACHE;

	if (megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS)
			!= DCMD_SUCCESS) {
		dev_info(&instance->pdev->dev,
			"return from %s %d \n", __func__, __LINE__);
		return;
	}

	megasas_return_cmd(instance, cmd);

	return;
}

/**
 * megasas_shutdown_controller -	Instructs FW to shutdown the controller
 * @instance:				Adapter soft state
 * @opcode:				Shutdown/Hibernate
 */
static void megasas_shutdown_controller(struct megasas_instance *instance,
					u32 opcode)
{
	struct megasas_cmd *cmd;
	struct megasas_dcmd_frame *dcmd;

	if (atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR)
		return;

	cmd = megasas_get_cmd(instance);

	if (!cmd)
		return;

	if (instance->aen_cmd)
		megasas_issue_blocked_abort_cmd(instance,
			instance->aen_cmd, MFI_IO_TIMEOUT_SECS);
	if (instance->map_update_cmd)
		megasas_issue_blocked_abort_cmd(instance,
			instance->map_update_cmd, MFI_IO_TIMEOUT_SECS);
	if (instance->jbod_seq_cmd)
		megasas_issue_blocked_abort_cmd(instance,
			instance->jbod_seq_cmd, MFI_IO_TIMEOUT_SECS);
	dcmd = &cmd->frame->dcmd;

	memset(dcmd->mbox.b, 0, MFI_MBOX_SIZE);

	dcmd->cmd = MFI_CMD_DCMD;
	dcmd->cmd_status = 0x0;
	dcmd->sge_count = 0;
	dcmd->flags = cpu_to_le16(MFI_FRAME_DIR_NONE);
	dcmd->timeout = 0;
	dcmd->pad_0 = 0;
	dcmd->data_xfer_len = 0;
	dcmd->opcode = cpu_to_le32(opcode);

	if (megasas_issue_blocked_cmd(instance, cmd, MFI_IO_TIMEOUT_SECS)
			!= DCMD_SUCCESS) {
		dev_info(&instance->pdev->dev,
			"return from %s %d \n", __func__, __LINE__);
		return;
	}

	megasas_return_cmd(instance, cmd);

	return;
}

#ifdef CONFIG_PM
/**
 * megasas_suspend -	driver suspend entry point
 * @pdev:		PCI device structure
 * @state:		PCI power state to suspend routine
 */
static int
megasas_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	int i;

	instance = pci_get_drvdata(pdev);
	host = instance->host;
	instance->unload = 1;

	/* Shutdown SR-IOV heartbeat timer */
	if (instance->requestorId && !instance->skip_heartbeat_timer_del)
		del_timer_sync(&instance->sriov_heartbeat_timer);

	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_HIBERNATE_SHUTDOWN);

	/* cancel the delayed work if this work still in queue */
	if (instance->ev != NULL) {
		struct megasas_aen_event *ev = instance->ev;
		cancel_delayed_work(&ev->hotplug_work);
		flush_scheduled_work();
		instance->ev = NULL;
	}

	tasklet_kill(&instance->isr_tasklet);

	pci_set_drvdata(instance->pdev, instance);
	instance->instancet->disable_intr(instance);

	if (instance->msix_vectors)
 		for (i = 0; i < instance->msix_vectors; i++) {
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 6)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
			if (smp_affinity_enable)
				irq_set_affinity_hint(
					instance->msixentry[i].vector, NULL);
#endif
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
		}
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);

	pci_save_state(pdev);
	pci_disable_device(pdev);

	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

/**
 * megasas_resume-      driver resume entry point
 * @pdev:               PCI device structure
 */
static int
megasas_resume(struct pci_dev *pdev)
{
	int rval;
	struct Scsi_Host *host;
	struct megasas_instance *instance;

	instance = pci_get_drvdata(pdev);
	host = instance->host;
	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);

	/*
	 * PCI prepping: enable device set bus mastering and dma mask
	 */
	rval = pci_enable_device(pdev);

	if (rval) {
		printk(KERN_ERR "megasas: Enable device failed\n");
		return rval;
	}

	pci_set_master(pdev);

	if (megasas_set_dma_mask(pdev))
		goto fail_set_dma_mask;

	/*
	 * We expect the FW state to be READY
	 */
	if (megasas_transition_to_ready(instance, 0))
		goto fail_ready_state;

	/* Now re-enable MSI-X */
	if (instance->msix_vectors)
		pci_enable_msix(instance->pdev, instance->msixentry, instance->msix_vectors);

	/*
	 * Initialize Firmware
	 */

	atomic_set(&instance->fw_outstanding,0);
	atomic_set(&instance->ldio_outstanding, 0);

	if (instance->ctrl_context) {
			megasas_reset_reply_desc(instance);
			if (megasas_ioc_init_fusion(instance)) {
				megasas_free_cmds(instance);
				megasas_free_cmds_fusion(instance);
				goto fail_init_mfi;
			}
			if (!megasas_get_map_info(instance))
				megasas_sync_map_info(instance);

	} else {
			*instance->producer = 0;
			*instance->consumer = 0;
			if ( megasas_check_cpx_support( instance ) == 0  ){
				if ( megasas_send_cpx_queue_data( instance ) ){
					printk("megasas: Sending cpx queue data to FW failed.\n");
					megasas_remove_cpx(instance);
				}else
					instance->cpx_request_queue->consumer_idx = instance->cpx_request_queue->producer_idx = 0;
			}
			if (megasas_issue_init_mfi(instance))
				goto fail_init_mfi;
	}

	tasklet_init(&instance->isr_tasklet, instance->instancet->tasklet,
                         (unsigned long)instance);	

	if (megasas_setup_irqs(instance, 0))
		goto fail_init_mfi;

	instance->instancet->enable_intr(instance);
	megasas_setup_jbod_map(instance);
	instance->unload = 0;

	/* Re-launch SR-IOV heartbeat timer */
	if (instance->requestorId) {
		if (!megasas_sriov_start_heartbeat(instance, 0))
			megasas_start_timer(instance,
					    &instance->sriov_heartbeat_timer,
					    megasas_sriov_heartbeat_handler,
					    MEGASAS_SRIOV_HEARTBEAT_INTERVAL_VF);
		else {
			instance->skip_heartbeat_timer_del = 1;
			goto fail_init_mfi;
		}
	}

	/*
	 * Initiate AEN (Asynchronous Event Notification)
	 */
	if (megasas_start_aen(instance))
		printk(KERN_ERR "megasas: Start AEN failed\n");

	return 0;

fail_init_mfi:
	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				instance->evt_detail,
				instance->evt_detail_h);
	if (instance->pd_info)
		pci_free_consistent(pdev, sizeof(struct MR_PD_INFO),
					instance->pd_info,
					instance->pd_info_h);

	if (instance->producer)
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
				instance->producer_h);
	if (instance->consumer)
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
				instance->consumer_h);

	scsi_host_put(host);

fail_set_dma_mask:
fail_ready_state:

	pci_disable_device(pdev);

	return -ENODEV;
}
#else
#define megasas_suspend	NULL
#define megasas_resume	NULL
#endif

inline static int megasas_wait_for_adapter_operational(struct megasas_instance *instance)
{
	int wait_time = MEGASAS_RESET_WAIT_TIME * 2;
	int i;

	for (i = 0; i < wait_time; i++) {
		if (atomic_read(&instance->adprecovery) == MEGASAS_HBA_OPERATIONAL)
			break;

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL))
			dev_notice(&instance->pdev->dev, "waiting for controller reset to finish\n");

		msleep(1000);
	}

	if (atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL) {
		dev_info(&instance->pdev->dev, "%s timed out while waiting for HBA to recover.\n", __FUNCTION__);
		return 1;
	}

	return 0;
}

/**
 * megasas_detach_one -	PCI hot"un"plug entry point
 * @pdev:		PCI device structure
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,10))
static void __devexit megasas_detach_one(struct pci_dev *pdev)
#else
static void megasas_detach_one(struct pci_dev *pdev)
#endif
{
	int i;
	struct Scsi_Host *host;
	struct megasas_instance *instance;
	struct fusion_context *fusion;
	u32 pd_seq_map_sz;

	instance = pci_get_drvdata(pdev);
	instance->unload = 1;
	host = instance->host;
	fusion = instance->ctrl_context;

	/* Shutdown SR-IOV heartbeat timer */
	if (instance->requestorId && !instance->skip_heartbeat_timer_del)
		del_timer_sync(&instance->sriov_heartbeat_timer);

	/* Free crash dump host buffers, if allocated */
	if (instance->fw_crash_state != UNAVAILABLE)
               	megasas_free_host_crash_buffer(instance); 

	scsi_remove_host(instance->host);

	msleep(1000);
	if ((atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR)
		|| ((atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL) &&
		megasas_wait_for_adapter_operational(instance)))
		goto skip_firing_dcmds;

	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);

skip_firing_dcmds:
	/* cancel the delayed work if this work still in queue*/
	if (instance->ev != NULL) {
		struct megasas_aen_event *ev = instance->ev;
		cancel_delayed_work(&ev->hotplug_work);
		flush_scheduled_work();
		instance->ev = NULL;
	}
	/* cancel all wait event */
	wake_up_all(&instance->int_cmd_wait_q);

	tasklet_kill(&instance->isr_tasklet);

	/*
	 * Take the instance off the instance array. Note that we will not
	 * decrement the max_index. We let this array be sparse array
	 */
	for (i = 0; i < megasas_mgmt_info.max_index; i++) {
		if (megasas_mgmt_info.instance[i] == instance) {
			megasas_mgmt_info.count--;
			megasas_mgmt_info.instance[i] = NULL;

			break;
		}
	}

	pci_set_drvdata(instance->pdev, NULL);

	instance->instancet->disable_intr(instance);

	if (instance->msix_vectors)
		for (i = 0; i < instance->msix_vectors; i++) {
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 6)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
			if (smp_affinity_enable)
				irq_set_affinity_hint(
					instance->msixentry[i].vector, NULL);
#endif
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
		}
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
	if (instance->is_ventura) {	 
		for (i = 0; i < MAX_LOGICAL_DRIVES_EXT; ++i)
			kfree(fusion->streamDetectByLD[i]);
		kfree(fusion->streamDetectByLD);
		fusion->streamDetectByLD = NULL;
	}

	if (instance->ctrl_context) {
		megasas_release_fusion(instance);
		pd_seq_map_sz = sizeof(struct MR_PD_CFG_SEQ_NUM_SYNC) +
			(sizeof(struct MR_PD_CFG_SEQ) *
				(MAX_PHYSICAL_DEVICES - 1));
		for (i = 0; i < 2 ; i++) {
			if (fusion->ld_map[i])
				dma_free_coherent(&instance->pdev->dev, fusion->max_map_sz,
						  fusion->ld_map[i], fusion->ld_map_phys[i]);
			if (fusion->ld_drv_map[i])
				free_pages((ulong)fusion->ld_drv_map[i], fusion->drv_map_pages);
			if (fusion->pd_seq_sync[i])
				dma_free_coherent(&instance->pdev->dev,
					pd_seq_map_sz,
					fusion->pd_seq_sync[i],
					fusion->pd_seq_phys[i]);
		}

		megasas_free_fusion_context(instance);
	} else {
		megasas_release_mfi(instance);
		pci_free_consistent(pdev, sizeof(u32), instance->producer,
					instance->producer_h);
		pci_free_consistent(pdev, sizeof(u32), instance->consumer,
					instance->consumer_h);
	}

	kfree(instance->ctrl_info);

	if (instance->evt_detail)
		pci_free_consistent(pdev, sizeof(struct megasas_evt_detail),
				instance->evt_detail, instance->evt_detail_h);

	if (instance->pd_info)
		pci_free_consistent(pdev, sizeof(struct MR_PD_INFO),
					instance->pd_info,
					instance->pd_info_h);

	if (instance->vf_affiliation)
		pci_free_consistent(pdev, (MAX_LOGICAL_DRIVES + 1) *
				    sizeof(struct MR_LD_VF_AFFILIATION),
				    instance->vf_affiliation,
				    instance->vf_affiliation_h);

	if (instance->vf_affiliation_111)
		pci_free_consistent(pdev,
				    sizeof(struct MR_LD_VF_AFFILIATION_111),
				    instance->vf_affiliation_111,
				    instance->vf_affiliation_111_h);

	if (instance->hb_host_mem)
		pci_free_consistent(pdev, sizeof(struct MR_CTRL_HB_HOST_MEM),
				    instance->hb_host_mem,
				    instance->hb_host_mem_h);

	if (instance->verbuf) {
		pci_free_consistent(pdev, MEGASAS_MAX_NAME*sizeof(u32), 
				    instance->verbuf, instance->verbuf_h);
	}

	if (instance->crash_dump_buf) {
		pci_free_consistent(pdev, CRASH_DMA_BUF_SIZE,
			    instance->crash_dump_buf, instance->crash_dump_h);
	}
	
	if (instance->system_info_buf) {
		pci_free_consistent(pdev, MR_DRV_SYSTEM_INFO_SIZE, 
				    instance->system_info_buf, instance->system_info_h);
	}

	scsi_host_put(host);

	pci_set_drvdata(pdev, NULL);

	pci_disable_device(pdev);

	return;
}

/**
 * megasas_shutdown -	Shutdown entry point
 * @device:		Generic device structure
 */
static void megasas_shutdown(struct pci_dev *pdev)
{
	int i;
	struct megasas_instance *instance = pci_get_drvdata(pdev);

	instance->unload = 1;
	
	msleep(1000);
	if ((atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR)
		|| ((atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL) &&
		megasas_wait_for_adapter_operational(instance)))
		goto skip_firing_dcmds;

	megasas_flush_cache(instance);
	megasas_shutdown_controller(instance, MR_DCMD_CTRL_SHUTDOWN);

skip_firing_dcmds:
	instance->instancet->disable_intr(instance);
	if (instance->msix_vectors)
		for (i = 0; i < instance->msix_vectors; i++) {
#if ((defined(RHEL_MAJOR) && (RHEL_MAJOR == 6)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
			if (smp_affinity_enable)
				irq_set_affinity_hint(
					instance->msixentry[i].vector, NULL);
#endif
			free_irq(instance->msixentry[i].vector, &instance->irq_context[i]);
		}
	else
		free_irq(instance->pdev->irq, &instance->irq_context[0]);
	if (instance->msix_vectors)
		pci_disable_msix(instance->pdev);
}

/**
 * megasas_mgmt_open -	char node "open" entry point
 */
static int megasas_mgmt_open(struct inode *inode, struct file *filep)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33))
	cycle_kernel_lock();
#endif
	/*
	 * Allow only those users with admin rights
	 */
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	return 0;
}

/**
 * megasas_mgmt_release - char node "release" entry point
 */
static int megasas_mgmt_release(struct inode *inode, struct file *filep)
{
	filep->private_data = NULL;
	fasync_helper(-1, filep, 0, &megasas_async_queue);

	return 0;
}

/**
 * megasas_mgmt_fasync -	Async notifier registration from applications
 *
 * This function adds the calling process to a driver global queue. When an
 * event occurs, SIGIO will be sent to all processes in this queue.
 */
static int megasas_mgmt_fasync(int fd, struct file *filep, int mode)
{
	int rc;

	mutex_lock(&megasas_async_queue_mutex);

	rc = fasync_helper(fd, filep, mode, &megasas_async_queue);

	mutex_unlock(&megasas_async_queue_mutex);

	if (rc >= 0) {
		/* For sanity check when we get ioctl */
		filep->private_data = filep;
		return 0;
	}

	printk(KERN_DEBUG "megasas: fasync_helper failed [%d]\n", rc);

	return rc;
}

/**
 * megasas_mgmt_poll -  char node "poll" entry point
 * */
static unsigned int megasas_mgmt_poll(struct file *file, poll_table *wait)
{
	unsigned int mask;
	unsigned long flags;
	poll_wait(file, &megasas_poll_wait, wait);
	spin_lock_irqsave(&poll_aen_lock, flags);
	if (megasas_poll_wait_aen){
		mask =   (POLLIN | POLLRDNORM);
	}
	else{
		mask = 0;
	}
	megasas_poll_wait_aen = 0;
	spin_unlock_irqrestore(&poll_aen_lock, flags);
	return mask;
}

/**
 * megasas_mgmt_fw_ioctl -	Issues management ioctls to FW
 * @instance:			Adapter soft state
 * @argp:			User's ioctl packet
 */
static int
megasas_mgmt_fw_ioctl(struct megasas_instance *instance,
		      struct megasas_iocpacket __user * user_ioc,
		      struct megasas_iocpacket *ioc)
{
	struct megasas_sge32 *kern_sge32;
	struct megasas_cmd *cmd;
	void *kbuff_arr[MAX_IOCTL_SGE];
	dma_addr_t buf_handle = 0;
	int error = 0, i;
	void *sense = NULL;
	dma_addr_t sense_handle;
	unsigned long *sense_ptr;
	struct megasas_instance *local_instance;

	memset(kbuff_arr, 0, sizeof(kbuff_arr));

	if (ioc->sge_count > MAX_IOCTL_SGE) {
		printk(KERN_DEBUG "megasas: SGE count [%d] >  max limit [%d]\n",
		       ioc->sge_count, MAX_IOCTL_SGE);
		return -EINVAL;
	}

	cmd = megasas_get_cmd(instance);
	if (!cmd) {
		printk(KERN_DEBUG "megasas: Failed to get a cmd packet\n");
		return -ENOMEM;
	}

	/*
	 * User's IOCTL packet has 2 frames (maximum). Copy those two
	 * frames into our cmd's frames. cmd->frame's context will get
	 * overwritten when we copy from user's frames. So set that value
	 * alone separately
	 */
	memcpy(cmd->frame, ioc->frame.raw, 2 * MEGAMFI_FRAME_SIZE);
	cmd->frame->hdr.context = cpu_to_le32(cmd->index);
	cmd->frame->hdr.pad_0 = 0;
	cmd->frame->hdr.flags &= cpu_to_le16(~(MFI_FRAME_IEEE |
					       MFI_FRAME_SGL64 |
					       MFI_FRAME_SENSE64));

	/* Check whether the DCMD sent is from crash dump utility*/
	if (cmd->frame->dcmd.opcode == MR_DRIVER_SET_APP_CRASHDUMP_MODE) {

		for (i = 0; i < megasas_mgmt_info.max_index; i++) {

			if (megasas_mgmt_info.instance[i]) {
				local_instance = megasas_mgmt_info.instance[i];
				if (local_instance->crash_dump_drv_support && 
					(atomic_read(&local_instance->adprecovery)
						== MEGASAS_HBA_OPERATIONAL) &&
					!megasas_set_crash_dump_params(local_instance,
						cmd->frame->dcmd.mbox.w[0])) {
					local_instance->crash_dump_app_support =
						cmd->frame->dcmd.mbox.w[0];
					dev_info(&local_instance->pdev->dev, "megasas: application crash dump mode set success\n");
					error = 0; /*SUCCESS*/
				} else {
					dev_info(&local_instance->pdev->dev,"megasas: application crash dump mode set failed\n");
					error = -1; /* FAILURE*/
				}
			}
        	}
		megasas_return_cmd(instance, cmd);
		return error;
	}	

	/*
	 * The management interface between applications and the fw uses
	 * MFI frames. E.g, RAID configuration changes, LD property changes
	 * etc are accomplishes through different kinds of MFI frames. The
	 * driver needs to care only about substituting user buffers with
	 * kernel buffers in SGLs. The location of SGL is embedded in the
	 * struct iocpacket itself.
	 */
	kern_sge32 = (struct megasas_sge32 *)
	    ((unsigned long)cmd->frame + ioc->sgl_off);

	/*
	 * For each user buffer, create a mirror buffer and copy in
	 */
	for (i = 0; i < ioc->sge_count; i++) {
		if (!ioc->sgl[i].iov_len)
			continue;

		kbuff_arr[i] = dma_alloc_coherent(&instance->pdev->dev,
						    ioc->sgl[i].iov_len,
						    &buf_handle, GFP_KERNEL);
		if (!kbuff_arr[i]) {
			printk(KERN_DEBUG "megasas: Failed to alloc "
			       "kernel SGL buffer for IOCTL \n");
			error = -ENOMEM;
			goto out;
		}

		/*
		 * We don't change the dma_coherent_mask, so
		 * pci_alloc_consistent only returns 32bit addresses
		 */
		kern_sge32[i].phys_addr = cpu_to_le32(buf_handle);
		kern_sge32[i].length = cpu_to_le32(ioc->sgl[i].iov_len);

		/*
		 * We created a kernel buffer corresponding to the
		 * user buffer. Now copy in from the user buffer
		 */
		if (copy_from_user(kbuff_arr[i], ioc->sgl[i].iov_base,
				   (u32) (ioc->sgl[i].iov_len))) {
			error = -EFAULT;
			goto out;
		}
	}

	if (ioc->sense_len) {
		sense = dma_alloc_coherent(&instance->pdev->dev, ioc->sense_len,
					     &sense_handle, GFP_KERNEL);
		if (!sense) {
			error = -ENOMEM;
			goto out;
		}

		sense_ptr =
		(unsigned long *) ((unsigned long)cmd->frame + ioc->sense_off);
		*sense_ptr = cpu_to_le32(sense_handle);
	}

	/*
	 * Set the sync_cmd flag so that the ISR knows not to complete this
	 * cmd to the SCSI mid-layer
	 */
	cmd->sync_cmd = 1;
	if (megasas_issue_blocked_cmd(instance, cmd, 0) == DCMD_NOT_FIRED) {
		cmd->sync_cmd = 0;
		dev_info(&instance->pdev->dev,
			"return -EBUSY from %s %d opcode 0x%x cmd->cmd_status_drv 0x%x \n",
			__func__, __LINE__, cmd->frame->dcmd.opcode,
			cmd->cmd_status_drv);
		return -EBUSY;
	}
	cmd->sync_cmd = 0;

	if (instance->unload == 1) {
		printk(KERN_INFO "megasas: we are doing unload so no need to submit data to" 
			"application. This may be force exit from driver \n");
		goto out;
	}

	if (cmd->frame->dcmd.opcode == MR_DCMD_CTRL_IO_METRICS_GET) {
		ProcessPerfMetricRequest(instance , kbuff_arr[0], ioc->sgl[0].iov_len);
	}

	/*
	 * copy out the kernel buffers to user buffers
	 */
	for (i = 0; i < ioc->sge_count; i++) {
		if (copy_to_user(ioc->sgl[i].iov_base, kbuff_arr[i],
				 ioc->sgl[i].iov_len)) {
			error = -EFAULT;
			printk(KERN_ERR "megasas: Failed to copy out to user "
					"kbuff\n");
			goto out;
		}
	}

	/*
	 * copy out the sense
	 */
	if (ioc->sense_len) {
		/*
		 * sense_buff points to the location that has the user
		 * sense buffer address
		 */
		sense_ptr = (unsigned long *) ((unsigned long)ioc->frame.raw +
				ioc->sense_off);

		if (copy_to_user((void __user *)((unsigned long)(*sense_ptr)),
				 sense, ioc->sense_len)) {
			printk(KERN_ERR "megasas: Failed to copy out to user "
					"sense data\n");
			error = -EFAULT;
			goto out;
		}
	}

	/*
	 * copy the status codes returned by the fw
	 */
	if (copy_to_user(&user_ioc->frame.hdr.cmd_status,
			 &cmd->frame->hdr.cmd_status, sizeof(u8))) {
		printk(KERN_DEBUG "megasas: Error copying out cmd_status\n");
		error = -EFAULT;
	}

      out:
	if (sense) {
		dma_free_coherent(&instance->pdev->dev, ioc->sense_len,
				    sense, sense_handle);
	}

	for (i = 0; i < ioc->sge_count; i++) {
		if (kbuff_arr[i]) {
			dma_free_coherent(&instance->pdev->dev,
				le32_to_cpu(kern_sge32[i].length),
				kbuff_arr[i],
				le32_to_cpu(kern_sge32[i].phys_addr));
			kbuff_arr[i] = NULL;
		} 
	}

	/* Complete both MPT and MFI command from this context 
	 * Applicable for Fusion Adapter only.
	 */
	megasas_return_cmd(instance, cmd);
	
	return error;
}

static int megasas_mgmt_ioctl_fw(struct file *file, unsigned long arg)
{
	struct megasas_iocpacket __user *user_ioc =
	    (struct megasas_iocpacket __user *)arg;
	struct megasas_iocpacket *ioc;
	struct megasas_instance *instance;
	int error;
	int i;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;

	ioc = kmalloc(sizeof(*ioc), GFP_KERNEL);
	if (!ioc)
		return -ENOMEM;

	if (copy_from_user(ioc, user_ioc, sizeof(*ioc))) {
		error = -EFAULT;
		goto out_kfree_ioc;
	}

	instance = megasas_lookup_instance(ioc->host_no);
	if (!instance) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

        /* Adjust ioctl wait time for VF mode */
        if (instance->requestorId)
                wait_time = MEGASAS_ROUTINE_WAIT_TIME_VF;

        /* Block ioctls in VF mode */
        if (instance->requestorId && !allow_vf_ioctls) {
                error = -ENODEV;
                goto out_kfree_ioc;
        }

	if (atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR) {
		printk("Controller in crit error\n");
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (instance->unload == 1) {
		error = -ENODEV;
		goto out_kfree_ioc;
	}

	if (down_interruptible(&instance->ioctl_sem)) {
		error = -ERESTARTSYS;
		goto out_kfree_ioc;
	}
	
	// If HBA is undergoing a reset recovery, wait for that to complete
	// before issuing this command

	for (i = 0; i < wait_time; i++) {

		if (atomic_read(&instance->adprecovery) == MEGASAS_HBA_OPERATIONAL)
			break;

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: waiting for controller reset to finish\n");
		}

		msleep(1000);
	}

	if (atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL) {
		printk("megaraid_sas: %s timed out while waiting for HBA to recover.\n", __FUNCTION__);
		error = -ENODEV;
		goto out_up;
	}

	error = megasas_mgmt_fw_ioctl(instance, user_ioc, ioc);
out_up:
	up(&instance->ioctl_sem);

out_kfree_ioc:
	kfree(ioc);
	return error;
}

static int megasas_mgmt_ioctl_aen(struct file *file, unsigned long arg)
{
	struct megasas_instance *instance;
	struct megasas_aen aen;
	int error;
	int i;
	u32 wait_time = MEGASAS_RESET_WAIT_TIME;

	if (file->private_data != file) {
		printk(KERN_DEBUG "megasas: fasync_helper was not "
		       "called first\n");
		return -EINVAL;
	}

	if (copy_from_user(&aen, (void __user *)arg, sizeof(aen)))
		return -EFAULT;

	instance = megasas_lookup_instance(aen.host_no);

	if (!instance)
		return -ENODEV;

	if (atomic_read(&instance->adprecovery) == MEGASAS_HW_CRITICAL_ERROR)
		return -ENODEV;

	if (instance->unload == 1) {
		return -ENODEV;
	}
	for (i = 0; i < wait_time; i++) {

		if (atomic_read(&instance->adprecovery) == MEGASAS_HBA_OPERATIONAL)
			break;

		if (!(i % MEGASAS_RESET_NOTICE_INTERVAL)) {
			printk(KERN_NOTICE "megasas: waiting for controller reset to finish\n");
		}

		msleep(1000);
	}

	if (atomic_read(&instance->adprecovery) != MEGASAS_HBA_OPERATIONAL) {
		printk("megaraid_sas: %s timed out while waiting for HBA to recover.\n", __FUNCTION__);
		return -ENODEV;
	}

	mutex_lock(&instance->reset_mutex);
	error = megasas_register_aen(instance, aen.seq_num,
				     aen.class_locale_word);
	mutex_unlock(&instance->reset_mutex);
	return error;
}

/**
 * megasas_mgmt_ioctl -	char node ioctl entry point
 */
static long
megasas_mgmt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MEGASAS_IOC_FIRMWARE:
		return megasas_mgmt_ioctl_fw(file, arg);

	case MEGASAS_IOC_GET_AEN:
		return megasas_mgmt_ioctl_aen(file, arg);
	}

	return -ENOTTY;
}

#ifdef CONFIG_COMPAT
static int megasas_mgmt_compat_ioctl_fw(struct file *file, unsigned long arg)
{
	struct compat_megasas_iocpacket __user *cioc =
	    (struct compat_megasas_iocpacket __user *)arg;
	struct megasas_iocpacket __user *ioc =
	    compat_alloc_user_space(sizeof(struct megasas_iocpacket));
	int i;
	int error = 0;
	compat_uptr_t ptr;
	u32 local_sense_off;
	u32 local_sense_len;
	u32 user_sense_off;

	if (clear_user(ioc, sizeof(*ioc)))
		return -EFAULT;

	if (copy_in_user(&ioc->host_no, &cioc->host_no, sizeof(u16)) ||
	    copy_in_user(&ioc->sgl_off, &cioc->sgl_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_off, &cioc->sense_off, sizeof(u32)) ||
	    copy_in_user(&ioc->sense_len, &cioc->sense_len, sizeof(u32)) ||
	    copy_in_user(ioc->frame.raw, cioc->frame.raw, 128) ||
	    copy_in_user(&ioc->sge_count, &cioc->sge_count, sizeof(u32)))
		return -EFAULT;

	/*
	 * The sense_ptr is used in megasas_mgmt_fw_ioctl only when
	 * sense_len is not null, so prepare the 64bit value under
	 * the same condition.
	 */
	if (get_user(local_sense_off, &ioc->sense_off) ||
		get_user(local_sense_len, &ioc->sense_len) ||
		get_user(user_sense_off, &cioc->sense_off))
		return -EFAULT;

	if (local_sense_len) {
		void __user **sense_ioc_ptr =
			(void __user **)((u8*)((unsigned long)&ioc->frame.raw) + local_sense_off);
		compat_uptr_t *sense_cioc_ptr =
			(compat_uptr_t *)(((unsigned long)&cioc->frame.raw) + user_sense_off);
		if (get_user(ptr, sense_cioc_ptr) ||
		    put_user(compat_ptr(ptr), sense_ioc_ptr))
			return -EFAULT;
	}

	for (i = 0; i < MAX_IOCTL_SGE; i++) {
		if (get_user(ptr, &cioc->sgl[i].iov_base) ||
		    put_user(compat_ptr(ptr), &ioc->sgl[i].iov_base) ||
		    copy_in_user(&ioc->sgl[i].iov_len,
				 &cioc->sgl[i].iov_len, sizeof(compat_size_t)))
			return -EFAULT;
	}

	error = megasas_mgmt_ioctl_fw(file, (unsigned long)ioc);

	if (copy_in_user(&cioc->frame.hdr.cmd_status,
			 &ioc->frame.hdr.cmd_status, sizeof(u8))) {
		printk(KERN_DEBUG "megasas: error copy_in_user cmd_status\n");
		return -EFAULT;
	}
	return error;
}

static long
megasas_mgmt_compat_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	switch (cmd) {
	case MEGASAS_IOC_FIRMWARE32:
		return megasas_mgmt_compat_ioctl_fw(file, arg);
	case MEGASAS_IOC_GET_AEN:
		return megasas_mgmt_ioctl_aen(file, arg);
	}

	return -ENOTTY;
}
#endif

/*
 * File operations structure for management interface
 */
static const struct file_operations megasas_mgmt_fops = {
	.owner = THIS_MODULE,
	.open = megasas_mgmt_open,
	.release = megasas_mgmt_release,
	.fasync = megasas_mgmt_fasync,
	.unlocked_ioctl = megasas_mgmt_ioctl,
	.poll = megasas_mgmt_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = megasas_mgmt_compat_ioctl,
#endif
};

/*
 * PCI hotplug support registration structure
 */
static struct pci_driver megasas_pci_driver = {

	.name = "megaraid_sas",
	.id_table = megasas_pci_table,
	.probe = megasas_probe_one,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,10))
	.remove = __devexit_p(megasas_detach_one),
#else
	.remove = megasas_detach_one,
#endif
	.suspend = megasas_suspend,
	.resume = megasas_resume,
	.shutdown = megasas_shutdown,
};

/*
 * Sysfs driver attributes
 */
static ssize_t megasas_sysfs_show_version(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_VERSION) + 2, "%s\n",
			MEGASAS_VERSION);
}

static DRIVER_ATTR(version, S_IRUGO, megasas_sysfs_show_version, NULL);

static ssize_t
megasas_sysfs_show_release_date(struct device_driver *dd, char *buf)
{
	return snprintf(buf, strlen(MEGASAS_RELDATE) + 2, "%s\n",
			MEGASAS_RELDATE);
}

static DRIVER_ATTR(release_date, S_IRUGO, megasas_sysfs_show_release_date,
		   NULL);

static ssize_t
megasas_sysfs_show_support_poll_for_event(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", support_poll_for_event);
}

static DRIVER_ATTR(support_poll_for_event, S_IRUGO,
			megasas_sysfs_show_support_poll_for_event, NULL);

static ssize_t
megasas_sysfs_show_support_device_change(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", support_device_change);
}

static DRIVER_ATTR(support_device_change, S_IRUGO,
			megasas_sysfs_show_support_device_change, NULL);

static ssize_t
megasas_sysfs_show_dbg_lvl(struct device_driver *dd, char *buf)
{
	return sprintf(buf, "%u\n", megasas_dbg_lvl);
}

static ssize_t
megasas_sysfs_set_dbg_lvl(struct device_driver *dd, const char *buf, size_t count)
{
	int retval = count;
	if(sscanf(buf,"%u",&megasas_dbg_lvl)<1){
		printk(KERN_ERR "megasas: could not set dbg_lvl\n");
		retval = -EINVAL;
	}
	return retval;
}

static DRIVER_ATTR(dbg_lvl, S_IRUGO|S_IWUSR, megasas_sysfs_show_dbg_lvl,
		megasas_sysfs_set_dbg_lvl);

#define SCAN_PD_CHANNEL	0x1
#define SCAN_VD_CHANNEL	0x2

static inline void megasas_remove_scsi_device(struct scsi_device *sdev)
{
	sdev_printk(KERN_INFO, sdev, "SCSI device is removed\n");
	scsi_remove_device(sdev);
	scsi_device_put(sdev);
}

static void
megasas_aen_polling(struct work_struct *work)
{
	struct 	Scsi_Host *host;
	struct 	scsi_device *sdev1;
	u16	pd_index = 0;
	u16	ld_index = 0;
	unsigned long flags;
	struct megasas_aen_event *ev =
		container_of(work, struct megasas_aen_event, hotplug_work.work);
	struct megasas_instance *instance = ev->instance;
	union megasas_evt_class_locale class_locale;
	int     i, j, doscan = 0;
	u32 seq_num, wait_time = MEGASAS_RESET_WAIT_TIME;
	int error;
	u8  dcmd_ret = DCMD_SUCCESS;


	if (!instance) {
		printk(KERN_ERR "invalid instance!\n");
		kfree(ev);
		return;
	}

	/* Adjust event workqueue thread wait time for VF mode */
	if (instance->requestorId)
		wait_time = MEGASAS_ROUTINE_WAIT_TIME_VF;

	/* Don't run the event workqueue thread if OCR is running */
	mutex_lock(&instance->reset_mutex);
	
	instance->ev = NULL;
	host = instance->host;
	if (instance->evt_detail) {
		megasas_decode_evt(instance);

		switch (le32_to_cpu(instance->evt_detail->code)) {

		case MR_EVT_PD_INSERTED:
		case MR_EVT_PD_REMOVED:
			dcmd_ret = megasas_get_pd_list(instance) ;
			if (dcmd_ret == DCMD_SUCCESS)
				doscan = SCAN_PD_CHANNEL;
			break;

		case MR_EVT_LD_OFFLINE:
		case MR_EVT_CFG_CLEARED:
		case MR_EVT_LD_DELETED:
		case MR_EVT_LD_CREATED:
			if (!instance->requestorId ||
				(instance->requestorId && megasas_get_ld_vf_affiliation(instance, 0)))
				dcmd_ret = megasas_ld_list_query(instance, MR_LD_QUERY_TYPE_EXPOSED_TO_HOST);
			
			if (dcmd_ret == DCMD_SUCCESS)
				doscan = SCAN_VD_CHANNEL;

			break;
		
		case MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED:
		case MR_EVT_FOREIGN_CFG_IMPORTED: 
		case MR_EVT_LD_STATE_CHANGE:
			dcmd_ret = megasas_get_pd_list(instance) ;
			
			if (dcmd_ret != DCMD_SUCCESS)
				break;
			
			if (!instance->requestorId ||
				(instance->requestorId && megasas_get_ld_vf_affiliation(instance, 0)))
				dcmd_ret = megasas_ld_list_query(instance, MR_LD_QUERY_TYPE_EXPOSED_TO_HOST);
			
			if (dcmd_ret != DCMD_SUCCESS)
				break;

			doscan = SCAN_VD_CHANNEL | SCAN_PD_CHANNEL;
			dev_info(&instance->pdev->dev, "scanning for scsi%d...\n",
				instance->host->host_no);
			break;
		
		case MR_EVT_CTRL_PERF_COLLECTION:
				spin_lock_irqsave(&instance->hba_lock, flags);
				ProcessPerfMetricAEN(instance , (MR_CTRL_IO_METRICS_CMD_TYPE)(instance->evt_detail->description[42] - 48));
				spin_unlock_irqrestore(&instance->hba_lock, flags);
				break;
		case MR_EVT_CTRL_PROP_CHANGED:
				dcmd_ret = megasas_get_ctrl_info(instance);
				break;
		default:
			doscan = 0;
			break;
		}
	} else {
		/* TBD. Most likely this condition will never hit */
		printk(KERN_ERR "invalid evt_detail!\n");
		mutex_unlock(&instance->reset_mutex);
		kfree(ev);
		return;
	}

	mutex_unlock(&instance->reset_mutex);
	
	if (doscan & SCAN_PD_CHANNEL) {
		for (i = 0; i < MEGASAS_MAX_PD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				pd_index = i*MEGASAS_MAX_DEV_PER_CHANNEL + j;
				sdev1 = scsi_device_lookup(host, i, j, 0);
				if (instance->pd_list[pd_index].driveState ==
							MR_PD_STATE_SYSTEM) {
					if (!sdev1)
						scsi_add_device(host, i, j, 0);
					else
						scsi_device_put(sdev1);
				} else {
					if (sdev1)
						megasas_remove_scsi_device(sdev1);
				}
			}
		}
	}

	if (doscan & SCAN_VD_CHANNEL) {
		for (i=0; i < MEGASAS_MAX_LD_CHANNELS; i++) {
			for (j = 0; j < MEGASAS_MAX_DEV_PER_CHANNEL; j++) {
				ld_index = (i * MEGASAS_MAX_DEV_PER_CHANNEL) + j;
				sdev1 = scsi_device_lookup(host, MEGASAS_MAX_PD_CHANNELS + i, j, 0);
				if (instance->ld_ids[ld_index] != 0xff) {
					if (!sdev1)
						scsi_add_device(host, MEGASAS_MAX_PD_CHANNELS + i, j, 0);
					else
						scsi_device_put(sdev1);
				} else {
					if (sdev1)
						megasas_remove_scsi_device(sdev1);
				}
			}
		}
	}

	if (dcmd_ret == DCMD_SUCCESS)
		seq_num = le32_to_cpu(instance->evt_detail->seq_num) + 1;
	else
		seq_num = instance->last_seq_num;

	/* Register AEN with FW for latest sequence number plus 1 */
	class_locale.members.reserved = 0;
	class_locale.members.locale = MR_EVT_LOCALE_ALL;
	class_locale.members.class = MR_EVT_CLASS_DEBUG;
	
	if ( instance->aen_cmd != NULL ) {
		kfree(ev);
		return;
	}

	mutex_lock(&instance->reset_mutex);
	error = megasas_register_aen(instance, seq_num,
					class_locale.word);
	if (error)
		printk(KERN_ERR "register aen failed error %x\n", error);

	mutex_unlock(&instance->reset_mutex);
	kfree(ev);
}

/**
 * megasas_init - Driver load entry point
 */
static int __init megasas_init(void)
{
	int rval;

	/* Reduce memory footprints if booted in kdump kernel */
	if (reset_devices) {
		msix_vectors = 1;
		rdpq_enable = 0;
		crashdump_enable = 0;
		dual_qdepth_disable = 1;
	}

	/*
	 * Announce driver version and other information
	 */
	printk(KERN_INFO "megasas: %s %s\n", MEGASAS_VERSION,
	       MEGASAS_EXT_VERSION);
	
	spin_lock_init(&poll_aen_lock);

	support_poll_for_event = 2;
	support_device_change = 1;

	memset(&megasas_mgmt_info, 0, sizeof(megasas_mgmt_info));

	/*
	 * Register character device node
	 */
	rval = register_chrdev(0, "megaraid_sas_ioctl", &megasas_mgmt_fops);

	if (rval < 0) {
		printk(KERN_DEBUG "megasas: failed to open device node\n");
		return rval;
	}

	megasas_mgmt_majorno = rval;

	/*
	 * Register ourselves as PCI hotplug module
	 */
	rval = pci_register_driver(&megasas_pci_driver);

	if (rval) {
		printk(KERN_DEBUG "megasas: PCI hotplug regisration failed \n");
		goto err_pcidrv;
	}

	megasas_ocr_wq = create_workqueue("megasas_ocr");

	if ( !megasas_ocr_wq )
		printk(KERN_DEBUG "megasas: Failed to create workqueue.\n");

	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_version);
	if (rval)
		goto err_dcf_attr_ver;
	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_release_date);
	if (rval)
		goto err_dcf_rel_date;

	rval = driver_create_file(&megasas_pci_driver.driver,
				&driver_attr_support_poll_for_event);
	if (rval)
		goto err_dcf_support_poll_for_event;

	rval = driver_create_file(&megasas_pci_driver.driver,
				  &driver_attr_dbg_lvl);
	if (rval)
		goto err_dcf_dbg_lvl;
	rval = driver_create_file(&megasas_pci_driver.driver,
				&driver_attr_support_device_change);
	if (rval)
		goto err_dcf_support_device_change;

	return rval;
err_dcf_support_device_change:
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
err_dcf_dbg_lvl:
	driver_remove_file(&megasas_pci_driver.driver,
			&driver_attr_support_poll_for_event);

err_dcf_support_poll_for_event:
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);

err_dcf_rel_date:
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);
err_dcf_attr_ver:
	pci_unregister_driver(&megasas_pci_driver);
err_pcidrv:
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
  	return rval;
}

/**
 * megasas_exit - Driver unload entry point
 */
static void __exit megasas_exit(void)
{
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_dbg_lvl);
	driver_remove_file(&megasas_pci_driver.driver,
			&driver_attr_support_poll_for_event);
	driver_remove_file(&megasas_pci_driver.driver,
			&driver_attr_support_device_change);
	driver_remove_file(&megasas_pci_driver.driver,
			   &driver_attr_release_date);
	driver_remove_file(&megasas_pci_driver.driver, &driver_attr_version);

	if (megasas_ocr_wq) {
		destroy_workqueue(megasas_ocr_wq);
		megasas_ocr_wq = NULL;
	}

	pci_unregister_driver(&megasas_pci_driver);
	unregister_chrdev(megasas_mgmt_majorno, "megaraid_sas_ioctl");
}

module_init(megasas_init);
module_exit(megasas_exit);
