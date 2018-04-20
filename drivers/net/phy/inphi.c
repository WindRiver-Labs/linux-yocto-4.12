/*
 *    Copyright 2018 NXP
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    Inphi is a registered trademark of Inphi Systems, Inc.
 *
 */
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#define PHY_ID_IN112525  0x02107440

#define INPHI_S03_DEVICE_ID_MSB 0x2
#define INPHI_S03_DEVICE_ID_LSB 0x3

#define INPHI_S03_MANUAL_RESET_CTRL_REG 0x2C

#define INPHI_S03_SOFT_RESET 0x200
#define INPHI_S03_PLL_LOCK_ACQUIRED 0x70 /* Rx lock, Tx lock, calibration done */
#define PLL_LOCK_RETRY_COUNTER 8
#define IRQ9  0
#define IRQ10 1

struct inphi_irq_mapping{
	const char* irq_dev_name;
	void (*warm_reset)(struct phy_device *phydev);
};

struct inphi_work_data{
    struct work_struct work;
    unsigned long      phy_dev;
    int                irq_pos;
    int                tx_pll_lock_counter;
};

static struct workqueue_struct *warm_reset_wq;
static struct inphi_work_data irq9_data = {0};
static struct inphi_work_data irq10_data = {0};

static void inphi_warm_reset_lane2(struct phy_device *phydev);
static void inphi_warm_reset_lane3(struct phy_device *phydev);

static struct inphi_irq_mapping irq_map[] = {
	[IRQ9]  = { "SFP2_RX_LOS", inphi_warm_reset_lane2},
	[IRQ10] = { "SFP3_RX_LOS", inphi_warm_reset_lane3},
};

static void inphi_warm_reset_lane2(struct phy_device *phydev) {
	u32 reg_value = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x323);

	if (!(reg_value & 0x8000)) {
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x388, 0x24);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x399, 0x12C);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x398, 0x3029);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x380, 0x418);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x3C7, 0x1000);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x3C7, 0x0);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x380, 0x10);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x380, 0x110);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x380, 0x10);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x398, 0x3028);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x38B, 0x3);
	}
}

static void inphi_warm_reset_lane3(struct phy_device *phydev) {
	u32 reg_value = phy_read_mmd(phydev, MDIO_MMD_VEND1, 0x423);

	if (!(reg_value & 0x8000)) {
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x488, 0x24);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x499, 0x12C);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x498, 0x3029);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x480, 0x418);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x4C7, 0x1000);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x4C7, 0x0);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x480, 0x10);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x480, 0x110);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x480, 0x10);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x498, 0x3028);
		phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x48B, 0x3);
	}
}

static int inphi_retry_tx_pll_lock(struct inphi_work_data *irqdata) {
	struct phy_device *phydev = (struct phy_device *)(irqdata->phy_dev);
	int reg_value;
	int ret = 0;

	if (!phydev) {
		ret = -ENXIO;
		goto exit_err;
	}

	if (!phydev->drv) {
		ret = -ENXIO;
		goto exit_err;
	}

	if (phydev->drv->phy_id != PHY_ID_IN112525) {
		dev_err(&phydev->mdio.dev,"Warm reset can only be applied to INPHI CDR PHY_ID: 0x%X\n",
		       PHY_ID_IN112525);
		ret = -ENXIO;
		goto exit_err;
	}

	reg_value = phy_read_mmd(phydev, MDIO_MMD_VEND1, INPHI_S03_MANUAL_RESET_CTRL_REG); /* LOL status */

	mdelay(700);

	while ((reg_value != INPHI_S03_PLL_LOCK_ACQUIRED) &&
	       (irqdata->tx_pll_lock_counter <= PLL_LOCK_RETRY_COUNTER)) {
		irqdata->tx_pll_lock_counter++;
		irq_map[irqdata->irq_pos].warm_reset(phydev);
		mdelay(100);
	}

exit_err:
	return ret;
}

static void inphi_irq_work_handler(struct work_struct *work) {
	struct inphi_work_data *data = (struct inphi_work_data *)work;
	data->tx_pll_lock_counter = 0;
	inphi_retry_tx_pll_lock(data);
}

static irqreturn_t inphi_sfp_irq_handler(int irq, void *dev) {
	struct work_struct *irq_queue_work = (struct work_struct *)dev;
	queue_work(warm_reset_wq, (struct work_struct *)irq_queue_work);
	return IRQ_HANDLED;
}

static int inphi_sfp_irq_setup(struct phy_device *phydev, struct inphi_work_data *irq_data) {
	struct platform_device *pdev = to_platform_device(&phydev->mdio.dev);
	struct device_node *phy_node = pdev->dev.of_node;
	int irq;
	int ret = 0;

	INIT_WORK((struct work_struct *)irq_data, inphi_irq_work_handler);
	irq_data->phy_dev = (unsigned long)phydev;

	irq = irq_of_parse_and_map(phy_node, irq_data->irq_pos);
	if (irq <= 0) {
		dev_err(&phydev->mdio.dev,
			"no 'interrupts' property in %s node\n",
			phy_node->name);
		ret = -1;
		goto err;
	}

	ret = irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
	if (ret < 0) {
		dev_err(&phydev->mdio.dev, "irq_set_irq_type() failed (ret = %d)\n", ret);
		goto err;
	}

	ret = request_irq(irq, inphi_sfp_irq_handler,
			  IRQF_NOBALANCING | IRQF_NO_THREAD,
			  irq_map[irq_data->irq_pos].irq_dev_name,
			  irq_data);
	if (ret < 0) {
		dev_err(&phydev->mdio.dev, "could not request irq %u (ret=%d)\n",
			irq, ret);
	}

err:
	return ret;
}

static int inphi_config_aneg(struct phy_device *phydev) {
	phydev->supported = SUPPORTED_10000baseT_Full;
	phydev->advertising = SUPPORTED_10000baseT_Full;
	return 0;
}

static int inphi_read_status(struct phy_device *phydev) {
	int reg_value = phy_read_mmd(phydev, MDIO_MMD_VEND1, INPHI_S03_MANUAL_RESET_CTRL_REG); /* LOL status */
	int ret = 0;

	if (reg_value < 0)
		goto err;

	if (reg_value == INPHI_S03_PLL_LOCK_ACQUIRED) {
		phydev->speed = SPEED_10000;
		phydev->duplex = DUPLEX_FULL;
		phydev->link = 1;
	} else {
		phydev->link = 0;
	}

err:
	return ret;
}

static int inphi_soft_reset(struct phy_device *phydev)
{
	phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x0, INPHI_S03_SOFT_RESET);
	return 0;
}

static int inphi_probe(struct phy_device *phydev) {
	u32 phy_id = 0;
	int id_lsb = 0, id_msb = 0;

	/* Read device id from phy registers. */
	id_lsb = phy_read_mmd(phydev, MDIO_MMD_VEND1, INPHI_S03_DEVICE_ID_MSB);
	if (id_lsb < 0)
		return -ENXIO;

	phy_id = id_lsb << 16;

	id_msb = phy_read_mmd(phydev, MDIO_MMD_VEND1, INPHI_S03_DEVICE_ID_LSB);
	if (id_msb < 0)
		return -ENXIO;

	phy_id |= id_msb;

	/* Make sure the device tree binding matched the driver with the
	 * right device.
	 */
	if (phy_id != phydev->drv->phy_id) {
		dev_err(&phydev->mdio.dev, "Error matching phy with %s driver\n",
			phydev->drv->name);
		return -ENODEV;
	}

	warm_reset_wq = create_workqueue("inphi_irq_wq");
	if (!warm_reset_wq) {
		dev_warn(&phydev->mdio.dev, "Work queue could not be created. Interrupt handler for warm reset will not work!\n");
		goto probe_exit;
	}

	irq9_data.irq_pos = IRQ9;
	if (inphi_sfp_irq_setup(phydev, &irq9_data)) {
		dev_err(&phydev->mdio.dev, "Error registering interrupt 9 for %s driver\n",
			phydev->drv->name);
	}

	irq10_data.irq_pos = IRQ10;
	if (inphi_sfp_irq_setup(phydev, &irq10_data)) {
		dev_err(&phydev->mdio.dev, "Error registering interrupt 10 for %s driver\n",
			phydev->drv->name);
	}

probe_exit:
	return 0;
}

static struct phy_driver inphi_driver[] = {
{
	.phy_id		= PHY_ID_IN112525,
	.phy_id_mask	= 0x0ff0fff0,
	.name		= "Inphi 112525_S03",
	.features	= PHY_GBIT_FEATURES,
	.config_aneg	= inphi_config_aneg,
	.read_status	= inphi_read_status,
	.soft_reset	= inphi_soft_reset,
	.probe		= inphi_probe,
},
};

module_phy_driver(inphi_driver);

static struct mdio_device_id __maybe_unused inphi_tbl[] = {
	{ PHY_ID_IN112525, 0x0ff0fff0},
	{},
};

MODULE_DEVICE_TABLE(mdio, inphi_tbl);
