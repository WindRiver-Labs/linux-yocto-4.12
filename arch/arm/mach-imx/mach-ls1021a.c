/*
 * Copyright 2013-2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <asm/mach/arch.h>
#include <linux/of_platform.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/sys_soc.h>
#include "common.h"

#ifdef CONFIG_FIXED_PHY
static int __init of_add_fixed_phys(void)
{
	int ret;
	struct device_node *np;
	u32 *fixed_link;
	struct fixed_phy_status status = {};

	for_each_node_by_name(np, "ethernet") {
		fixed_link  = (u32 *)of_get_property(np, "fixed-link", NULL);
		if (!fixed_link)
			continue;

		status.link = 1;
		status.duplex = be32_to_cpu(fixed_link[1]);
		status.speed = be32_to_cpu(fixed_link[2]);
		status.pause = be32_to_cpu(fixed_link[3]);
		status.asym_pause = be32_to_cpu(fixed_link[4]);

		ret = fixed_phy_add(PHY_POLL, be32_to_cpu(fixed_link[0]), &status, -1);
		if (ret) {
			of_node_put(np);
			return ret;
		}
	}

	return 0;
}
arch_initcall(of_add_fixed_phys);
#endif /* CONFIG_FIXED_PHY */

#define	DCFG_CCSR_SVR 0x0A4

/**
 * ls1021a_get_revision - Get ls1021a silicon revision
 *
 * Return: Silicon version or -1 otherwise
 */
static int __init ls1021a_get_revision(void)
{
	struct device_node *np;
	void __iomem *ls1021a_devcfg_base;
	u32 revision;

	np = of_find_compatible_node(NULL, NULL, "fsl,ls1021a-dcfg");
	if (!np) {
		pr_err("%s: no devcfg node found\n", __func__);
		return -1;
	}

	ls1021a_devcfg_base = of_iomap(np, 0);
	of_node_put(np);
	if (!ls1021a_devcfg_base) {
		pr_err("%s: Unable to map I/O memory\n", __func__);
		return -1;
	}

	revision = ioread32be(ls1021a_devcfg_base + DCFG_CCSR_SVR) & 0x1f;

	iounmap(ls1021a_devcfg_base);

	return revision;
}

struct device * __init ls1021a_soc_device_init(void)
{
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device_node *root;
	int ret;
	u32 revision;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return NULL;

	soc_dev_attr->family = "Freescale LS 1021A";

	root = of_find_node_by_path("/");
	ret = of_property_read_string(root, "model", &soc_dev_attr->machine);
	of_node_put(root);
	if (ret)
		goto free_soc;

	revision = ls1021a_get_revision();
	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "%d.%d", (revision >> 4) & 0xf, revision & 0xf);

	if (!soc_dev_attr->revision)
		goto free_soc;

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev))
		goto free_rev;

	return soc_device_to_device(soc_dev);

free_rev:
	kfree(soc_dev_attr->revision);
free_soc:
	kfree(soc_dev_attr);
	return NULL;
}

static void __init ls1021a_init_machine(void)
{
	struct device *parent;

	parent = ls1021a_soc_device_init();
	if (parent == NULL)
		pr_warn("failed to initialize soc device\n");
}

static const char * const ls1021a_dt_compat[] __initconst = {
	"fsl,ls1021a",
	NULL,
};

DT_MACHINE_START(LS1021A, "Freescale LS1021A")
	.smp		= smp_ops(ls1021a_smp_ops),
	.init_machine   = ls1021a_init_machine,
	.dt_compat	= ls1021a_dt_compat,
MACHINE_END
