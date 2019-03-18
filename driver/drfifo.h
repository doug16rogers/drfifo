/* Copyright (c) 2013-2019 Doug Rogers under the Zero Clause BSD License. */
/* You are free to do whatever you want with this software. See LICENSE.txt. */

#ifndef __drfifo_h__
#define __drfifo_h__

#include <ntddk.h>

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD     drfifo_unload;

DRIVER_DISPATCH drfifo_handle_irp_default;

__drv_dispatchType(IRP_MJ_CREATE)
DRIVER_DISPATCH drfifo_handle_irp_create;

__drv_dispatchType(IRP_MJ_READ)
DRIVER_DISPATCH drfifo_handle_irp_read;

__drv_dispatchType(IRP_MJ_WRITE)
DRIVER_DISPATCH drfifo_handle_irp_write;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH drfifo_handle_irp_ioctl;

__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH drfifo_handle_irp_close;


#endif
