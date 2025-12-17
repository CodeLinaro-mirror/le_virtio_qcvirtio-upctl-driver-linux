/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UPCTL_CORE_H
#define UPCTL_CORE_H

#define UPCTL_MAX_PINCTRLS 48

struct device;

/**
 * struct upctl_device_proto_ops - represents the various operations provided
 * by upctl-device. Currently there is only one callback.
 *
 * @select_state: performs pinctrl state selection on given device.
 */

struct upctl_device_proto_ops {
	int (*select_state)(struct device *dev, const char *state_name);
};

/**
 * upctl_core_register_state - Register pinctrl in upctrl-core.
 * @dev: Device
 * @state_name: Pinctrl state name
 * @ops: Callbacks to perform operations on the device.
 *
 */

int upctl_core_register_state(struct device *dev, const char *state_name,
			      struct upctl_device_proto_ops *ops);

#endif /* UPCTL_CORE_H */
