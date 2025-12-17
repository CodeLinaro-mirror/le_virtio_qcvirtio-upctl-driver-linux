/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>

#include "upctl-core.h"

static struct upctl_device_proto_ops upctl_dev_ops;

static int upctl_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i, num_pinctrl_names;
	struct device_node *of_node = pdev->dev.of_node;
	struct property *pinctrl_names;
	const char *state_name;
	struct pinctrl *p;
	struct pinctrl_state *state;

	dev_dbg(&pdev->dev, "upctl-device probe function called\n");

	pinctrl_names = of_find_property(of_node, "pinctrl-names", NULL);
	if (!pinctrl_names) {
		dev_err(&pdev->dev, "%s has no 'pinctrl-names' property.\n",
			of_node->name);
		return -EINVAL;
	}

	num_pinctrl_names = of_property_count_strings(of_node, "pinctrl-names");

	if (num_pinctrl_names == 0) {
		dev_info(&pdev->dev, "No pin controls configured.\n");
		return -EINVAL;
	}

	if (num_pinctrl_names > UPCTL_MAX_PINCTRLS)
		num_pinctrl_names = UPCTL_MAX_PINCTRLS;

	p = pinctrl_get(&pdev->dev);

	if (IS_ERR(p)) {
		dev_err(&pdev->dev, "Could not get pinctrl device\n");
		return PTR_ERR(p);
	}

	for (i = 0; i < num_pinctrl_names; i++) {
		of_property_read_string_index(of_node, "pinctrl-names", i,
					      &state_name);

		if (IS_ERR(state_name)) {
			dev_err(&pdev->dev,
				"Could not obtain name pinctrl name at index %d.\n",
				i);
			return PTR_ERR(state_name);
		}

		state = pinctrl_lookup_state(p, state_name);

		if (IS_ERR(state)) {
			dev_err(&pdev->dev,
				"Could not lookup '%s' pinctrl state\n",
				state_name);
			return PTR_ERR(state);
		}

		ret = upctl_core_register_state(&pdev->dev, state_name,
						&upctl_dev_ops);

		if (ret) {
			dev_err(&pdev->dev, "Could not register state: '%s",
				state_name);
			return ret;
		}
	}

	return ret;
}

static int upctl_device_select_state(struct device *dev, const char *state_name)
{
	struct pinctrl *p = pinctrl_get_select(dev, state_name);

	if (IS_ERR(p)) {
		dev_err(dev, "Could not select '%s' pinctrl state.\n",
			state_name);
		return PTR_ERR(p);
	}
	dev_dbg(dev, "SELECT_STATE: '%s'\n", state_name);

	return 0;
}

static int upctl_device_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "upctl-device remove function called\n");
	return 0;
}

static const struct of_device_id upctl_device_of_match[] = {
	{
		.compatible = "coqos,upctl-device",
	},
	{
		.compatible = "opsy,upctl-device",
	},
	{},
};

MODULE_DEVICE_TABLE(of, upctl_device_of_match);

static struct platform_driver upctl_device_driver = {
	.driver = {
		.name = "upctl-device",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(upctl_device_of_match),
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
	},
	.probe = upctl_device_probe,
	.remove = upctl_device_remove,
};

static struct upctl_device_proto_ops upctl_dev_ops = {
	.select_state = upctl_device_select_state,
};

static int __init upctl_device_init(void)
{
	return platform_driver_register(&upctl_device_driver);
}

static void __exit upctl_device_exit(void)
{
	platform_driver_unregister(&upctl_device_driver);
}

module_init(upctl_device_init);
module_exit(upctl_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pinctrl device driver");
