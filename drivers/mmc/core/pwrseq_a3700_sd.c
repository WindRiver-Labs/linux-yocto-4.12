/*
 * Driver for Marvell armada3700 SD power sequence
 *
 * Copyright (C) 2017 Marvell, All Rights Reserved.
 *
 * Author:      Zhoujie Wu <zjwu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>

#include <linux/mmc/host.h>

#include "pwrseq.h"

struct mmc_pwrseq_a3700 {
	struct mmc_pwrseq pwrseq;
	struct gpio_desc *pwren_gpio;
};

static void mmc_pwrseq_a3700_pre_power_on(struct mmc_host *host)
{
	struct mmc_pwrseq_a3700 *pwrseq = container_of(host->pwrseq,
					struct mmc_pwrseq_a3700, pwrseq);

	gpiod_set_value_cansleep(pwrseq->pwren_gpio, 0);
	msleep(50);
}

static void mmc_pwrseq_a3700_post_power_on(struct mmc_host *host)
{
	struct mmc_pwrseq_a3700 *pwrseq = container_of(host->pwrseq,
					struct mmc_pwrseq_a3700, pwrseq);

	gpiod_set_value_cansleep(pwrseq->pwren_gpio, 1);
}

static struct mmc_pwrseq_ops mmc_pwrseq_a3700_ops = {
	.pre_power_on = mmc_pwrseq_a3700_pre_power_on,
	.post_power_on = mmc_pwrseq_a3700_post_power_on,
};

static int mmc_pwrseq_a3700_sd_probe(struct platform_device *pdev)
{
	struct mmc_pwrseq_a3700 *pwrseq;
	struct device *dev = &pdev->dev;

	pwrseq = devm_kzalloc(dev, sizeof(*pwrseq), GFP_KERNEL);
	if (!pwrseq)
		return -ENOMEM;

	pwrseq->pwren_gpio = devm_gpiod_get(dev, "pwren", GPIOD_OUT_HIGH);
	if (IS_ERR(pwrseq->pwren_gpio))
		return PTR_ERR(pwrseq->pwren_gpio);

	pwrseq->pwrseq.ops = &mmc_pwrseq_a3700_ops;
	pwrseq->pwrseq.dev = dev;
	pwrseq->pwrseq.owner = THIS_MODULE;
	platform_set_drvdata(pdev, pwrseq);

	return mmc_pwrseq_register(&pwrseq->pwrseq);
}

static int mmc_pwrseq_a3700_sd_remove(struct platform_device *pdev)
{
	struct mmc_pwrseq_a3700 *pwrseq = platform_get_drvdata(pdev);

	mmc_pwrseq_unregister(&pwrseq->pwrseq);

	return 0;
}

static const struct of_device_id mmc_pwrseq_a3700_sd_of_match[] = {
	{ .compatible = "mmc-pwrseq-a3700-sd",},
	{/* sentinel */},
};

MODULE_DEVICE_TABLE(of, mmc_pwrseq_a3700_sd_of_match);

static struct platform_driver mmc_pwrseq_a3700_sd_driver = {
	.probe = mmc_pwrseq_a3700_sd_probe,
	.remove = mmc_pwrseq_a3700_sd_remove,
	.driver = {
		.name = "pwrseq_a3700_sd",
		.of_match_table = mmc_pwrseq_a3700_sd_of_match,
	},
};

module_platform_driver(mmc_pwrseq_a3700_sd_driver);
MODULE_LICENSE("GPL v2");
