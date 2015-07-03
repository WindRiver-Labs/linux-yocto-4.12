/*
 * CPUFreq support for Armada 370/XP platforms.
 *
 * Copyright (C) 2012-2016 Marvell
 *
 * Yehuda Yitschak <yehuday@marvell.com>
 * Gregory Clement <gregory.clement@free-electrons.com>
 * Thomas Petazzoni <thomas.petazzoni@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#define pr_fmt(fmt) "mvebu-pmsu: " fmt

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/resource.h>

extern int (*mvebu_pmsu_dfs_request_ptr)(int cpu);
extern int armada_xp_pmsu_dfs_request(int cpu);
extern int armada_38x_pmsu_dfs_request(int cpu);

static int __init mvebu_pmsu_cpufreq_init(void)
{
	struct device_node *np;
	struct resource res;
	int ret, cpu;

	if (!of_machine_is_compatible("marvell,armadaxp") &&
	    !of_machine_is_compatible("marvell,armada380"))
		return 0;

	/*
	 * In order to have proper cpufreq handling, we need to ensure
	 * that the Device Tree description of the CPU clock includes
	 * the definition of the PMU DFS registers. If not, we do not
	 * register the clock notifier and the cpufreq driver. This
	 * piece of code is only for compatibility with old Device
	 * Trees.
	 */
	np = of_find_compatible_node(NULL, NULL, "marvell,armada-xp-cpu-clock");
	if (!np)
		return 0;

	ret = of_address_to_resource(np, 1, &res);
	if (ret) {
		pr_warn(FW_WARN "not enabling cpufreq, deprecated armada-xp-cpu-clock binding\n");
		of_node_put(np);
		return 0;
	}

	of_node_put(np);

	/*
	 * For each CPU, this loop registers the operating points
	 * supported (which are the nominal CPU frequency and half of
	 * it), and registers the clock notifier that will take care
	 * of doing the PMSU part of a frequency transition.
	 */
	for_each_possible_cpu(cpu) {
		struct device *cpu_dev;
		struct clk *clk;
		int ret;

		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			pr_err("Cannot get CPU %d\n", cpu);
			continue;
		}

		clk = clk_get(cpu_dev, NULL);
		if (IS_ERR(clk)) {
			pr_err("Cannot get clock for CPU %d\n", cpu);
			return PTR_ERR(clk);
		}

		clk_prepare_enable(clk);

		/*
		 * In case of a failure of dev_pm_opp_add(), we don't
		 * bother with cleaning up the registered OPP (there's
		 * no function to do so), and simply cancel the
		 * registration of the cpufreq device.
		 */
		ret = dev_pm_opp_add(cpu_dev, clk_get_rate(clk), 0);
		if (ret) {
			clk_put(clk);
			return ret;
		}

		ret = dev_pm_opp_add(cpu_dev, clk_get_rate(clk) / 2, 0);
		if (ret) {
			clk_put(clk);
			return ret;
		}

		ret = dev_pm_opp_set_sharing_cpus(cpu_dev,
						  cpumask_of(cpu_dev->id));
		if (ret)
			dev_err(cpu_dev, "%s: failed to mark OPPs as shared: %d\n",
				__func__, ret);
	}

	mvebu_pmsu_dfs_request_ptr = armada_xp_pmsu_dfs_request;

	if (of_machine_is_compatible("marvell,armada380")) {
		if (num_online_cpus() == 1)
			mvebu_v7_pmsu_disable_dfs_cpu(1);

		mvebu_pmsu_dfs_request_ptr = armada_38x_pmsu_dfs_request;
	} else if (of_machine_is_compatible("marvell,armadaxp")) {
		mvebu_pmsu_dfs_request_ptr = armada_xp_pmsu_dfs_request;
	}
	platform_device_register_simple("cpufreq-dt", -1, NULL, 0);

	return 0;
}
device_initcall(mvebu_pmsu_cpufreq_init);
