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
#include <linux/timer.h>
#include <linux/delay.h>

#define PHY_ID_IN112525  0x02107440

#define INPHI_S03_DEVICE_ID_MSB 0x2
#define INPHI_S03_DEVICE_ID_LSB 0x3

#define ALL_LANES		    4
#define INPHI_POLL_DELAY	    2500

#define L0_LANE_CONTROL             16
#define L1_LANE_CONTROL             17
#define L2_LANE_CONTROL             18
#define L3_LANE_CONTROL             19
#define P_LANE_CONTROL              20
#define LOL_STATUS                  33
#define LOS_STATUS                  34
#define FIFO_CONTROL                37
#define MANUAL_RESET_CONTROL        44
#define MISC_CONTROL                0xb3

#define EFUSE_STATUS_REG            1536

/* bit 8 is cal done  bits 5:0 are calibration value */
#define L0_TX_LDRV_VREG_OBSERVE     0x116
#define L1_TX_LDRV_VREG_OBSERVE     0x216
#define L2_TX_LDRV_VREG_OBSERVE     0x316
#define L3_TX_LDRV_VREG_OBSERVE     0x416

#define L0_TX_PLL_CTRL_1            0x120
#define L1_TX_PLL_CTRL_1            0x220
#define L2_TX_PLL_CTRL_1            0x320
#define L3_TX_PLL_CTRL_1            0x420

#define L0_TX_PLL_CTRL_2            0x121
#define L1_TX_PLL_CTRL_2            0x221
#define L2_TX_PLL_CTRL_2            0x321
#define L3_TX_PLL_CTRL_2            0x421

#define L0_TX_PLL_STATUS            0x123
#define L1_TX_PLL_STATUS            0x223
#define L2_TX_PLL_STATUS            0x323
#define L3_TX_PLL_STATUS            0x423

/* bit 8 is cal done  bits 5:0 are calibration value */
#define L0_TX_PLL_VREG_OBSERVE      0x130
#define L1_TX_PLL_VREG_OBSERVE      0x230
#define L2_TX_PLL_VREG_OBSERVE      0x330
#define L3_TX_PLL_VREG_OBSERVE      0x430

#define L0_RX_MAIN_CTRL             0x180
#define L1_RX_MAIN_CTRL             0x280
#define L2_RX_MAIN_CTRL             0x380
#define L3_RX_MAIN_CTRL             0x480

#define L0_RX_FREQ_ACQ_CTRL_1       0x181
#define L1_RX_FREQ_ACQ_CTRL_1       0x281
#define L2_RX_FREQ_ACQ_CTRL_1       0x381
#define L3_RX_FREQ_ACQ_CTRL_1       0x481

#define L0_RX_OSC_STS_1             0x19D
#define L1_RX_OSC_STS_1             0x29D
#define L2_RX_OSC_STS_1             0x39D
#define L3_RX_OSC_STS_1             0x49D

#define L0_RX_OSC_MAN_CTRL_1        0x198

#define L0_RX_OSC_MAN_CTRL_2        0x199
#define L1_RX_OSC_MAN_CTRL_2        0x299
#define L2_RX_OSC_MAN_CTRL_2        0x399
#define L3_RX_OSC_MAN_CTRL_2        0x499

#define L0_RX_PHASE_ADJUST          0x18B
#define L1_RX_PHASE_ADJUST          0x28B
#define L2_RX_PHASE_ADJUST          0x38B
#define L3_RX_PHASE_ADJUST          0x48B

/* bit 8 is cal done  bits 5:0 are calibration value */
#define L0_RX_VREG1_OBSERVE         0x1a8
#define L1_RX_VREG1_OBSERVE         0x2a8
#define L2_RX_VREG1_OBSERVE         0x3a8
#define L3_RX_VREG1_OBSERVE         0x4a8

#define L0_RX_AMUX_TMODE_CTRL       0x1C7

#define L0_TX_SWING_CONTROL	    0x102
#define P_TX_SWING_CONTROL	    0x502

#define P_RX_MAIN_CONTROL           0x580
#define P_RX_FREQ_ACQ_CTRL_1        0x581
#define P_RX_PHASE_ADJUST           0x58B
#define P_RX_OSC_MAN_CTRL_1         0x598
#define P_RX_SA_OFFSET_CONTROL_ALL  0x5C4
#define P_RX_AMUX_TMODE_CTRL        0x5C7

#define L0_RX_CTL                   0x1C8
#define L1_RX_CTL                   0x2C8
#define L2_RX_CTL                   0x3C8
#define L3_RX_CTL                   0x4C8

#define P_RX_OSC_MAN_CTRL_5         0x59c
#define P_TX_PLL_CTRL_1             0x520
#define P_TX_PLL_CTRL_2             0x521

#define L2_TX_PLL_CTRL_2            0x321

#define L0_RX_SA_E0_OFFSET_CONTROL  0x1B0
#define L0_RX_SA_E1_OFFSET_CONTROL  0x1B1
#define L0_RX_SA_E2_OFFSET_CONTROL  0x1B2
#define L0_RX_SA_E3_OFFSET_CONTROL  0x1B3
#define L0_RX_SA_H0_OFFSET_CONTROL  0x1BC
#define L0_RX_SA_H1_OFFSET_CONTROL  0x1BD
#define L0_RX_SA_H2_OFFSET_CONTROL  0x1BE
#define L0_RX_SA_H3_OFFSET_CONTROL  0x1BF
#define L0_RX_SA_ES_OFFSET_CONTROL  0x1C0

/* exit error codes */
#define CAL_NOT_DONE                0x8
#define AZ_FAILED                   0x10
#define TX_PLL_NOT_LOCKED           0x20
#define EFUSE_NOT_COMPLETE          0x40
#define VREG_NOT_COMPLETE           0x80
#define LANE_INVALID                0x2

/* Other constants */
#define RX_PHASE_ADJUST_TRACK_VAL       36
#define RX_VCO_CODE_OFFSET               5
#define WATCH_VCO_INTERVAL          0xffff
#define LOL_PERSIST_TIME               700
#define LOL_LOCK_TIME                  100

#define mdio_wr(a,b) phy_write_mmd(inphi_phydev, MDIO_MMD_VEND1, (a), (b))
#define mdio_rd(a)   phy_read_mmd(inphi_phydev, MDIO_MMD_VEND1, (a))

#define L0_VCO_CODE_TRIM  390
#define L1_VCO_CODE_TRIM  390
#define L2_VCO_CODE_TRIM  390
#define L3_VCO_CODE_TRIM  390

int vco_codes[ALL_LANES] = {
			L0_VCO_CODE_TRIM,
			L1_VCO_CODE_TRIM,
			L2_VCO_CODE_TRIM,
			L3_VCO_CODE_TRIM };

static void mykmod_work_handler(struct work_struct *w);

static struct workqueue_struct *wq;
static DECLARE_DELAYED_WORK(mykmod_work, mykmod_work_handler);
static unsigned long onesec;
struct phy_device *inphi_phydev;

int bit_test(int value, int bit_field);
void tx_restart(int lane);
void disable_lane(int lane);
void rx_powerdown_assert(int lane);
void rx_powerdown_de_assert(int lane);
void rx_reset_assert(int lane);
void rx_reset_de_assert(int lane);
void toggle_reset(int lane);

void WAIT(int delay_cycles)
{
	udelay(delay_cycles * 10);
}


int tx_pll_lock_test(int lane)
{
	int i, val, locked = 1;

	if (lane == ALL_LANES) {
		for (i = 0; i < ALL_LANES; i++) {
			val = mdio_rd(i * 0x100 + L0_TX_PLL_STATUS);
			locked = locked & bit_test(val, 15);
		}
	} else {
		val = mdio_rd(lane * 0x100 + L0_TX_PLL_STATUS);
		locked = locked & bit_test(val, 15);
	}

	return locked;
}

void tx_pll_assert(int lane)
{
	int val, recal;

	if (lane == ALL_LANES) {
		val = mdio_rd(MANUAL_RESET_CONTROL);
		recal = (1<<12);
		mdio_wr(MANUAL_RESET_CONTROL, val | recal);
	}
	else {
		val = mdio_rd(lane * 0x100 + L0_TX_PLL_CTRL_2);
		recal = (1<<15);
		mdio_wr(lane * 0x100 + L0_TX_PLL_CTRL_2, val | recal);
}
}

void tx_pll_de_assert(int lane)
{
	int recal, val;

	if (lane == ALL_LANES) {
		val = mdio_rd( MANUAL_RESET_CONTROL);
		recal = 0xefff; /* bit 12 set to 0 will deassert tx pll reset */
		mdio_wr(MANUAL_RESET_CONTROL, val & recal);
	} else {
		val = mdio_rd( lane*0x100 + L0_TX_PLL_CTRL_2);
		recal = 0x7fff; /* bit 15 is a 0 to de-assert PLL reset */
		mdio_wr(lane * 0x100 + L0_TX_PLL_CTRL_2, val & recal);
	}
}

void tx_core_assert(int lane)
{
	int recal, val, val2, core_reset;

	if (lane == 4) {
		val = mdio_rd(MANUAL_RESET_CONTROL);
		recal = 1<<10; /* bit 10 controls core datapath */
		mdio_wr(MANUAL_RESET_CONTROL, val | recal);
	} else {
		/* read, modify, write operation with bits 8-11 */
		val2 = mdio_rd(MISC_CONTROL);
		core_reset = (1 << (lane + 8));
		mdio_wr(MISC_CONTROL, val2 | core_reset);
	}
}

void lol_disable(int lane)
{
	int val, mask;

	/* bits 4-7 reset lanes 0-3 */
        val = mdio_rd(MISC_CONTROL);
        mask = 1 << (lane + 4);
        mdio_wr( MISC_CONTROL, val | mask);
}

void tx_core_de_assert(int lane)
{
	int val, recal, val2, core_reset;

	if (lane == ALL_LANES) {
		val = mdio_rd( MANUAL_RESET_CONTROL);
		recal = 0xffff - (1<<10);
		mdio_wr( MANUAL_RESET_CONTROL, val & recal);
	} else {
		val2 = mdio_rd(MISC_CONTROL);
		core_reset = 0xffff - (1 << (lane + 8));
		mdio_wr(MISC_CONTROL, val2 & core_reset);
	}
}


void tx_restart(int lane)
{
	tx_core_assert(lane);
	tx_pll_assert(lane);
	tx_pll_de_assert(lane);
	/* wait 1.5 ms before de-asserting core reset */
	WAIT(150);
	tx_core_de_assert(lane);
}

void disable_lane(int lane)
{
	rx_reset_assert(lane);
	rx_powerdown_assert(lane);
	tx_core_assert(lane);
	lol_disable(lane);
}    

int bit_test(int value, int bit_field)
{
	int bit_mask = (1 << bit_field);
	int result;
	result = ((value & bit_mask) == bit_mask);
	return result;
}

void toggle_reset(int lane)
{
	int reg, val, orig;

	if (lane == ALL_LANES) {
		mdio_wr(MANUAL_RESET_CONTROL, 0x8000);
		WAIT(10);
		mdio_wr(MANUAL_RESET_CONTROL, 0x0000);
	} else {
		reg = lane * 0x100 + L0_RX_CTL;
		val = (1 << 6);
		orig = mdio_rd(reg);
		mdio_wr(reg, orig + val);
		WAIT(10);
		mdio_wr(reg, orig);
	}
}

int az_complete_test(int lane)
{
	int success = 1, value;

	if (lane == 0 || lane == ALL_LANES) {
		value = mdio_rd(L0_RX_MAIN_CTRL);
		success = success & bit_test(value, 2);
	}
	if (lane == 1 || lane == ALL_LANES) {
		value = mdio_rd(L1_RX_MAIN_CTRL);
		success = success & bit_test(value, 2);
	}
	if (lane == 2 || lane == ALL_LANES) {
		value = mdio_rd(L2_RX_MAIN_CTRL);
		success = success & bit_test(value, 2);
	}
	if (lane == 3 || lane == ALL_LANES) {
		value = mdio_rd(L3_RX_MAIN_CTRL);
		success = success & bit_test(value, 2);
	}

	return success;
}

void rx_reset_assert(int lane)
{
	int mask, val;

	if (lane == ALL_LANES) {
		val = mdio_rd(MANUAL_RESET_CONTROL);
		mask = (1<<15);
		mdio_wr( MANUAL_RESET_CONTROL, val + mask);
	} else {
		val = mdio_rd(lane * 0x100 + L0_RX_CTL);
		mask = (1 << 6);
		mdio_wr(lane * 0x100 + L0_RX_CTL, val + mask);
	}
}

void rx_reset_de_assert(int lane)
{
	int mask, val;

	if (lane == ALL_LANES) {
		val = mdio_rd( MANUAL_RESET_CONTROL);
		mask = 0xffff - (1<<15);
		mdio_wr( MANUAL_RESET_CONTROL, val & mask);
	}
	else {
		val = mdio_rd( lane*0x100 + L0_RX_CTL);
		mask = 0xffff - (1<<6);
		mdio_wr( lane*0x100 + L0_RX_CTL, val & mask);
	}
}

void rx_powerdown_assert(int lane)
{
	int mask,val;

        val = mdio_rd(lane * 0x100 + L0_RX_CTL);
        mask = (1 << 5);
        mdio_wr(lane * 0x100 + L0_RX_CTL, val + mask);
}

void rx_powerdown_de_assert(int lane)
{
	int mask,val;

        val = mdio_rd(lane * 0x100 + L0_RX_CTL);
        mask = 0xffff - (1<<5);
        mdio_wr(lane * 0x100 + L0_RX_CTL, val & mask);
}

void save_az_offsets(int lane)
{
	int i;

	/* copy the auto-zero offset to the auto-zero offset override field
	 * (byte swap of register; bits involved: 15:8 --> 7:0
	 */
#define AZ_OFFSET_LANE_UPDATE(reg, lane) \
	mdio_wr((reg) + (lane) * 0x100,  \
		(mdio_rd((reg) + (lane) * 0x100) >> 8))

	if (lane == ALL_LANES) {
		for (i = 0; i < ALL_LANES; i++) {
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E0_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E1_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E2_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E3_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H0_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H1_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H2_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H3_OFFSET_CONTROL, i);
			AZ_OFFSET_LANE_UPDATE(L0_RX_SA_ES_OFFSET_CONTROL, i);
		}
	} else {
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E0_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E1_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E2_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_E3_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H0_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H1_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H2_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_H3_OFFSET_CONTROL, lane);
		AZ_OFFSET_LANE_UPDATE(L0_RX_SA_ES_OFFSET_CONTROL, lane);
	}

	/* it is mandatory to enable the offset ovveride bit for all lanes*/
	mdio_wr(P_RX_SA_OFFSET_CONTROL_ALL, 0x0001);
}

void save_vco_codes(int lane)
{
	int i;

	/* read the current adapted VCO code for lane, then save it into the
	 * trim register in RX_OSC_MAN_CTRL_2 after adding the VCO increment
	 */
	if (lane == ALL_LANES) {
		for (i = 0; i < ALL_LANES; i++) {
			vco_codes[i] = mdio_rd(L0_RX_OSC_STS_1 + i * 0x100);
			mdio_wr(L0_RX_OSC_STS_1 + i * 0x100,
				vco_codes[i] + RX_VCO_CODE_OFFSET);
		}
	} else {
		vco_codes[lane] = mdio_rd(L0_RX_OSC_STS_1 + lane * 0x100);
		mdio_wr(L0_RX_OSC_STS_1 + lane * 0x100,
			vco_codes[i] + RX_VCO_CODE_OFFSET);
	}
}

int inphi_lane_recovery(int lane)
{
	int i, value, az_pass;

	switch (lane) {
	case 0:
	case 1:
	case 2:
	case 3:
		/* reset lanes individually */
		rx_reset_assert(lane);
		WAIT(2000);
		break;
	case ALL_LANES:
		/* start with RX, TX, TXPLL & Core in reset */
		mdio_wr(MANUAL_RESET_CONTROL, 0x9C00);
		WAIT(2000);
		/* Check manual reset control register for regulator calibration
		   done, and eventually loop back until the voltage regulators
		   are up and calibration is done */
		do {
			value = mdio_rd(MANUAL_RESET_CONTROL);
			WAIT(1);
		} while (!bit_test(value, 4));
		break;
	default:
		dev_err(&inphi_phydev->mdio.dev,
			"Incorrect usage of APIs in %s driver\n",
			inphi_phydev->drv->name);
		break;
	}

	/* start with RX VCO codes with their fuse-trimmed values, knowing that
	 * this sets the center of the freq aquisition sweep
	 */
	if (lane == ALL_LANES) {
		for (i = 0; i < ALL_LANES; i++)
			mdio_wr(L0_RX_OSC_MAN_CTRL_2 + i * 0x100,
				L0_VCO_CODE_TRIM);
	}
	else
		mdio_wr(L0_RX_OSC_MAN_CTRL_2 + lane * 0x100, L0_VCO_CODE_TRIM);

	/* reset RX and run auto-zero calibration on the sense-amps */
	if (lane == ALL_LANES)
		for (i = 0; i < ALL_LANES; i++)
			mdio_wr(L0_RX_MAIN_CTRL + i * 0x100, 0x0418);
	else
		mdio_wr(L0_RX_MAIN_CTRL + lane * 0x100, 0x0418);

	/* disable the SA offset override for auto-zero calibration */
	mdio_wr(P_RX_SA_OFFSET_CONTROL_ALL,   0x0000);

	/* de-assert RX reset, while keeping the TX, PLL and CORE in reset */
	rx_reset_de_assert(lane);

	/* force RX auto-zero calibration */
	if (lane == ALL_LANES) {
		for (i = 0; i < ALL_LANES; i++) {
			mdio_wr(L0_RX_MAIN_CTRL + i * 0x100, 0x0410);
			mdio_wr(L0_RX_MAIN_CTRL + i * 0x100, 0x0412);
		}
	} else {
		mdio_wr(L0_RX_MAIN_CTRL + lane * 0x100, 0x0410);
		mdio_wr(L0_RX_MAIN_CTRL + lane * 0x100, 0x0412);
	}

	/* loop to check timeout on auto-zero test completion */
	for (i = 0; i < 64; i++) {
		WAIT(10000);
		az_pass = az_complete_test(lane);
		if (az_pass) {
			save_az_offsets(lane);
			break;
		}
		else {
			/* there is no point in continuing */
			pr_info("AZ timed out for lane %d\n", lane);
			return AZ_FAILED;
		}
	}

	/* run the freq acquisition on the RX */
	if (lane == ALL_LANES) {
		mdio_wr(P_RX_FREQ_ACQ_CTRL_1, 0x0002);
		mdio_wr(P_RX_OSC_MAN_CTRL_1, 0x2028);
		mdio_wr(P_RX_MAIN_CONTROL, 0x0010);
		WAIT(100);
		mdio_wr(P_RX_MAIN_CONTROL, 0x0110);
		WAIT(3000);
		mdio_wr(P_RX_OSC_MAN_CTRL_1, 0x3020);
	} else {
		mdio_wr(L0_RX_FREQ_ACQ_CTRL_1 + lane * 0x100, 0x0002);
		mdio_wr(L0_RX_OSC_MAN_CTRL_1 + lane * 0x100, 0x2028);
		mdio_wr(L0_RX_MAIN_CTRL + lane * 0x100, 0x0010);
		WAIT(100);
		mdio_wr(L0_RX_MAIN_CTRL + lane * 0x100, 0x0110);
		WAIT(3000);
		mdio_wr(L0_RX_OSC_MAN_CTRL_1 + lane * 0x100, 0x3020);
	}

	/* toggle TX pll reset and check for lock */
	if (lane == ALL_LANES) {
		/* assert & de-assert tx pll reset */
		mdio_wr( MANUAL_RESET_CONTROL, 0x1C00);
		mdio_wr( MANUAL_RESET_CONTROL, 0x0C00);
	}
	else {
		tx_restart(lane);
		WAIT(1100);
	}

	/* check the aggregated PLL lock bit in the manual reset control reg
	 * if the freq acquisition block aquired a signal, then the TX PLL
	 * should lock to it; it not, there is no point in continuing
	 */
	if (lane == ALL_LANES) {
		if (bit_test( mdio_rd(MANUAL_RESET_CONTROL), 6) == 0) {
		    //pr_info("TX pll is not locked so exit\n");
		    return TX_PLL_NOT_LOCKED;
		}
	}
	else {
		if (tx_pll_lock_test(lane) == 0) {
			return TX_PLL_NOT_LOCKED;
		}
	}

	/* since we're here, it means that the PLL locked; saving VCO codes */
	save_vco_codes(lane);

	/* de-asert the TX serdes reset; enable TX on desired lane(s) */
	if (lane == ALL_LANES) {
		mdio_wr(MANUAL_RESET_CONTROL,      0x0400);
		mdio_wr( MANUAL_RESET_CONTROL,      0x0000);
		value = mdio_rd( L2_LANE_CONTROL);
		value = value & 0xffbf; /* Clear bit 6 to enable TX */
		mdio_wr( P_LANE_CONTROL, value);
	}
	else {
		tx_core_de_assert(lane);
	}

	/* reset the core FIFOs and clear any previous FIFO error */
	if (lane == ALL_LANES) {
		mdio_wr( FIFO_CONTROL, 0x8000);
		mdio_wr( FIFO_CONTROL, 0x0000);
	}
	mdio_rd(FIFO_CONTROL);
	mdio_rd(FIFO_CONTROL);

	/* are we running in OTU mode ?! */
	WAIT(100);

	/* Dummy read the LOL and LOS status to clear any initial flags */
	mdio_rd(LOL_STATUS);
	mdio_rd(LOS_STATUS);

	return 0;
}

static void mykmod_work_handler(struct work_struct *w)
{
	u32 reg_value;
	int lane0_lock, lane1_lock, lane2_lock, lane3_lock;
	int all_lanes_lock;
        reg_value = phy_read_mmd(inphi_phydev, MDIO_MMD_VEND1, 0x123);
        lane0_lock = reg_value & (1<<15);
        reg_value = phy_read_mmd(inphi_phydev, MDIO_MMD_VEND1, 0x223);
        lane1_lock = reg_value & (1<<15);
        reg_value = phy_read_mmd(inphi_phydev, MDIO_MMD_VEND1, 0x323);
        lane2_lock = reg_value & (1<<15);
        reg_value = phy_read_mmd(inphi_phydev, MDIO_MMD_VEND1, 0x423);
	lane3_lock = reg_value & (1<<15);

	all_lanes_lock = lane0_lock | lane1_lock | lane2_lock | lane3_lock;

	if (!all_lanes_lock) {
		inphi_lane_recovery(ALL_LANES);
	} else {
		if (!lane0_lock)
			inphi_lane_recovery(0);
		if (!lane1_lock)
			inphi_lane_recovery(1);
		if (!lane2_lock)
			inphi_lane_recovery(2);
		if (!lane3_lock)
			inphi_lane_recovery(3);
	}

	queue_delayed_work(wq, &mykmod_work, onesec);
}

int inphi_probe(struct phy_device *phydev)
{
	u32 phy_id = 0, id_lsb = 0, id_msb = 0;

	/* Read device id from phy registers */
	id_lsb = phy_read_mmd(phydev, MDIO_MMD_VEND1, INPHI_S03_DEVICE_ID_MSB);
	if (id_lsb < 0)
		return -ENXIO;

	phy_id = id_lsb << 16;

	id_msb = phy_read_mmd(phydev, MDIO_MMD_VEND1, INPHI_S03_DEVICE_ID_LSB);
	if (id_msb < 0)
		return -ENXIO;

	phy_id |= id_msb;

	/*
	 * Make sure the device tree binding matched the driver with the
	 * right device.
	 */
	if (phy_id != phydev->drv->phy_id) {
		dev_err(&phydev->mdio.dev,
			"Error matching phy with %s driver\n",
			phydev->drv->name);
		return -ENODEV;
	}

	/* update the local phydev pointer, used inside all APIs */
        inphi_phydev = phydev;

	/* start fresh */
	/* inphi_lane_recovery(ALL_LANES); */

        onesec = msecs_to_jiffies(INPHI_POLL_DELAY);

	wq = create_singlethread_workqueue("inphi_kmod");
	if (wq)
		queue_delayed_work(wq, &mykmod_work, onesec);
	else {
		dev_err(&phydev->mdio.dev,
			"Error creating kernel workqueue for %s driver\n",
			phydev->drv->name);
		return -ENOMEM;
	}

	return 0;
}

static struct phy_driver inphi_driver[] = {
{
	.phy_id		= PHY_ID_IN112525,
	.phy_id_mask	= 0x0ff0fff0,
	.name		= "Inphi 112525_S03",
	.features	= PHY_GBIT_FEATURES,
	.probe		= &inphi_probe,
},
};

module_phy_driver(inphi_driver);

static struct mdio_device_id __maybe_unused inphi_tbl[] = {
	{ PHY_ID_IN112525, 0x0ff0fff0},
	{},
};

MODULE_DEVICE_TABLE(mdio, inphi_tbl);

MODULE_DESCRIPTION("Inphi IN112525_S03 driver");
MODULE_AUTHOR("Florin Chiculita");
MODULE_LICENSE("GPL");
