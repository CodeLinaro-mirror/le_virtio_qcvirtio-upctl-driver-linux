/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only with Linux-syscall-note
 */

#ifndef UPCTL_IOCTL_H
#define UPCTL_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define UPCTL_NAME_MAX_SIZE 64

#define UPCTLIO 'P'
#define UPCTL_IOCTL_GET_DEVICES_COUNT _IOR(UPCTLIO, 0x00, __u32)
#define UPCTL_IOCTL_GET_STATES_COUNT _IOR(UPCTLIO, 0x01, __u32)
#define UPCTL_IOCTL_SELECT_STATE _IOW(UPCTLIO, 0x02, __u32)

#endif /* UPCTL_IOCTL_H */
