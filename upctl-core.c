/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <uapi/linux/upctl_ioctl.h>
#include "upctl-core.h"

struct upctl_pinctrl_entry {
	char *name;
	struct device *dev;
	struct upctl_device_proto_ops *ops;
};

struct upctl_core_device {
	unsigned int devices_count;
	unsigned int states_count;
	struct upctl_pinctrl_entry pinctrls[UPCTL_MAX_PINCTRLS];
	struct miscdevice miscdev;
};

static char *upctl_dev_kstrndup(struct device *dev, const char *src,
				size_t max_len, gfp_t gfp)
{
	char *dst;
	size_t size;

	if (!src)
		return NULL;

	size = strnlen(src, max_len) + 1;

	dst = devm_kmalloc(dev, size, gfp);

	if (dst) {
		memcpy(dst, src, size);
		dst[size - 1] = '\0';
	}

	return dst;
}

int upctl_core_register_state(struct device *dev, const char *state_name,
			      struct upctl_device_proto_ops *ops)
{
	unsigned int id;
	struct platform_device *pdev =
		container_of(dev->parent, struct platform_device, dev);
	struct upctl_core_device *upctl_dev = platform_get_drvdata(pdev);

	if (upctl_dev->states_count >= UPCTL_MAX_PINCTRLS)
		return -ENOMEM;

	id = upctl_dev->states_count;
	upctl_dev->pinctrls[id].dev = dev;
	upctl_dev->pinctrls[id].name = upctl_dev_kstrndup(
		&pdev->dev, state_name, UPCTL_NAME_MAX_SIZE, GFP_KERNEL);

	if (!upctl_dev->pinctrls[id].name)
		return -ENOMEM;

	upctl_dev->pinctrls[id].ops = ops;
	upctl_dev->states_count++;

	dev_info(dev, "registered state [%u] - %s\n", id, state_name);

	return 0;
}
EXPORT_SYMBOL_GPL(upctl_core_register_state);

static long upctl_ioctl_get_devices_count(struct upctl_core_device *upctl_dev,
					  unsigned int __user *arg)
{
	if (copy_to_user(arg, &upctl_dev->devices_count, sizeof(*arg)))
		return -EFAULT;

	return 0;
}

static long upctl_ioctl_get_states_count(struct upctl_core_device *upctl_dev,
					 unsigned int __user *arg)
{
	if (copy_to_user(arg, &upctl_dev->states_count, sizeof(*arg)))
		return -EFAULT;

	return 0;
}

static long upctl_ioctl_select_state(struct upctl_core_device *upctl_dev,
				     unsigned int __user *arg)
{
	unsigned int state_id;
	struct upctl_pinctrl_entry *state_entry;

	if (copy_from_user(&state_id, arg, sizeof(state_id)))
		return -EFAULT;

	if (state_id >= upctl_dev->states_count)
		return -EINVAL;

	state_entry = &upctl_dev->pinctrls[state_id];

	if (!state_entry->ops->select_state)
		return -EINVAL;

	return state_entry->ops->select_state(state_entry->dev,
					      state_entry->name);
}

static long upctl_core_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	struct miscdevice *upctl_miscdev = file->private_data;
	struct platform_device *pdev = container_of(
		upctl_miscdev->parent, struct platform_device, dev);
	struct upctl_core_device *upctl_dev = platform_get_drvdata(pdev);

	switch (cmd) {
	case UPCTL_IOCTL_GET_DEVICES_COUNT:
		return upctl_ioctl_get_devices_count(
			upctl_dev, (unsigned int __user *)arg);
	case UPCTL_IOCTL_GET_STATES_COUNT:
		return upctl_ioctl_get_states_count(upctl_dev,
						    (unsigned int __user *)arg);
	case UPCTL_IOCTL_SELECT_STATE:
		return upctl_ioctl_select_state(upctl_dev,
						(unsigned int __user *)arg);
	default:
		break;
	}

	return -EINVAL;
}

static const struct file_operations upctl_core_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = upctl_core_ioctl
};

static int __init upctl_core_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct upctl_core_device *upctl_dev;
	unsigned int num_dev_nodes = 0;
	struct device_node *of_node = pdev->dev.of_node;
	struct device_node *np = NULL;

	if (!of_node) {
		if (of_have_populated_dt())
			dev_dbg(&pdev->dev,
				"of_node is NULL - not parsing upctl DT\n");
		return 0;
	}

	dev_dbg(&pdev->dev, "Iterating over child devices:\n");

	for_each_child_of_node(of_node, np) {
		num_dev_nodes++;
		dev_dbg(&pdev->dev, "    %s\n", np->name);
	}

	dev_dbg(&pdev->dev, "---------------- TOTAL: %u\n", num_dev_nodes);
	upctl_dev = devm_kzalloc(&pdev->dev, sizeof(*upctl_dev), GFP_KERNEL);
	if (!upctl_dev) {
		dev_err(&pdev->dev, "Memory allocation error.\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, upctl_dev);

	upctl_dev->devices_count = num_dev_nodes;
	upctl_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	upctl_dev->miscdev.name = "upctl";
	upctl_dev->miscdev.fops = &upctl_core_fops;
	upctl_dev->miscdev.parent = &pdev->dev;

	ret = misc_register(&upctl_dev->miscdev);

	if (ret) {
		dev_err(upctl_dev->miscdev.this_device,
			"failed to register misc device %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "registered '%s' %d:%d\n", upctl_dev->miscdev.name,
		 MISC_MAJOR, upctl_dev->miscdev.minor);
	ret = of_platform_populate(of_node, NULL, NULL, &pdev->dev);

	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int __exit upctl_core_remove(struct platform_device *pdev)
#else
static void upctl_core_remove(struct platform_device *pdev)
#endif
{
	struct upctl_core_device *upctl_dev = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "removing");
	misc_deregister(&upctl_dev->miscdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static const struct of_device_id upctl_dt_match[] = {
	{ .compatible = "coqos,upctl-core" },
	{ .compatible = "opsy,upctl-core" },
	{}
};

static struct platform_driver upctl_core_driver = {
	.driver = {
		.name = "upctl-core",
		.of_match_table = upctl_dt_match,
	},
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	.remove = __exit_p(upctl_core_remove),
#else
	.remove = upctl_core_remove,
#endif
};

module_platform_driver_probe(upctl_core_driver, upctl_core_probe);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pinctrl core driver");
