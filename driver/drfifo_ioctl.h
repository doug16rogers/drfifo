/* Copyright (c) 2013-2019 Doug Rogers under the Zero Clause BSD License. */
/* You are free to do whatever you want with this software. See LICENSE.txt. */

#ifndef __drfifo_ioctl_h__
#define __drfifo_ioctl_h__

#if defined(WINDDK) || defined(NT_INST)
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include "drfifo_stdint.h"

/**
 * Device-specific ID for this device. It is included in ioctl() command
 * definitions. This is a stolen and modified version from mktools'
 * ioctlcmd.h. I do not know how the originals were created.
 */
#define FILE_DEVICE_DRFIFO   0x00B770FC

/**
 * Resets the FIFO by setting both the put and get pointers to 0.
 *
 * The optional argument structure drfifo_ioctl_reset_t new_size field may be
 * used to set a new FIFO length, in bytes. Note that the length must be a
 * power of two for proper wrapping at the word size boundary (2^32 or 2^64).
 */
#define DRFIFO_IOCTL_RESET      ((ulong_t) CTL_CODE(FILE_DEVICE_DRFIFO, 0x01, METHOD_BUFFERED, FILE_WRITE_ACCESS))

/**
 * Flushes the FIFO by making the get pointer equal the put pointer.
 */
#define DRFIFO_IOCTL_FLUSH      ((ulong_t) CTL_CODE(FILE_DEVICE_DRFIFO, 0x02, METHOD_BUFFERED, FILE_WRITE_ACCESS))

/**
 * Retrieves the status of the FIFO. See structure drfifo_ioctl_status_t.
 */
#define DRFIFO_IOCTL_STATUS     ((ulong_t) CTL_CODE(FILE_DEVICE_DRFIFO, 0x03, METHOD_BUFFERED, FILE_READ_ACCESS))

/* #define IOCTL_TRANSFER_TYPE( _iocontrol)   (_iocontrol & 0x3) */

/**
 * Optional argument structure for DRFIFO_IOCTL_RESET.
 */
typedef struct drfifo_ioctl_reset_s
{
    size_t new_size;     /**< New size for the FIFO, in bytes. 0 means no new size. */
} drfifo_ioctl_reset_t;

/**
 * Argument structure for DRFIFO_IOCTL_STATUS.
 */
typedef struct drfifo_ioctl_status_s
{
    size_t size;        /**< Size of the FIFO, in bytes. */
    size_t flags;       /**< Flags for FIFO (currently not defined). */
    size_t put_count;   /**< Number of bytes so far written to the FIFO. */
    size_t get_count;   /**< Number of bytes so far read from the FIFO. */
} drfifo_ioctl_status_t;

/**
 * A union over all the ioctl() argument structures, if that's how you prefer
 * to work.
 */
typedef union drfifo_ioctl_arg_u
{
    drfifo_ioctl_reset_t  reset;
    drfifo_ioctl_status_t status;
} drfifo_ioctl_arg_t;

#endif
