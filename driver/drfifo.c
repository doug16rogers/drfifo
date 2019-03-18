/* Copyright (c) 2013-2019 Doug Rogers under the Zero Clause BSD License. */
/* You are free to do whatever you want with this software. See LICENSE.txt. */

#include <ntifs.h>
//#include <ntddk.h>
#include <wdm.h>
#include <wdmsec.h>      // PRV, for IoCreateDeviceSecure
#include <ntstrsafe.h>

#include "drfifo.h"
#include "drfifo_stdint.h"
#include "drfifo_ioctl.h"
#include "fifo.h"

//#ifdef UNICODE
#define M_T(_s) L ## _s
//#else
//#define M_T(_s) _s
//#endif

/**
 * The name of this driver.
 */
#define DRIVER_NAME   "drfifo"

/**
 * The name of this driver, possibly in wide char.
 */
#define T_DRIVER_NAME   M_T("drfifo")

/**
 * The name of our device.
 */
const WCHAR DRFIFO_DEVICE_NAME[]  = M_T("\\Device\\") T_DRIVER_NAME;

/**
 * A link to the device.
 */
const WCHAR DRFIFO_DEVICE_LINK[]  = M_T("\\??\\") T_DRIVER_NAME;

/*
 * These appear in the sample drivers.
 */
#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
//#pragma alloc_text (PAGE, drfifo_handle_irp_default)
//#pragma alloc_text (PAGE, drfifo_handle_irp_create)
//#pragma alloc_text (PAGE, drfifo_handle_irp_read)
//#pragma alloc_text (PAGE, drfifo_handle_irp_write)
//#pragma alloc_text (PAGE, drfifo_handle_irp_ioctl)
//#pragma alloc_text (PAGE, drfifo_handle_irp_close)
#endif

/**
 * GUID for our driver.
 *
 * To generate, run:
 *   '/c/Program Files (x86)/Microsoft Visual Studio 9.0/Common7/Tools/guidgen.exe'
 */
static const GUID guid_drfifo =      // {28329D26-B481-41dd-8F6D-77F68CBF2883}
{ 0x28329d26, 0xb481, 0x41dd, { 0x8f, 0x6d, 0x77, 0xf6, 0x8c, 0xbf, 0x28, 0x83 } };

/**
 * Structure holding private data for a device that's handled by our driver.
 */
typedef struct drfifo_dev_s
{
    KSPIN_LOCK   lock;          /**< General lock, used mostly to protect the FIFO. */
    fifo_t*      fifo;          /**< FIFO object. */
    PIO_WORKITEM work_item;     /**< Work item for writing to file. */
} drfifo_dev_t;

/**
 * Global pointer to our singleton device object.
 *
 * @todo Remove this and create one per opened device.
 */
PDEVICE_OBJECT g_dev = NULL;

/* ------------------------------------------------------------------------- */
/**
 * Provides a protected FIFO put operation for the @a drfifo device
 * (extension).
 *
 * @param drfifo - device of interest.
 * @param data - pointer to data to be put into the FIFO.
 * @param size - number of bytes to put into the FIFO.
 *
 * @return the actual number of bytes written to the FIFO.
 */
static ssize_t drfifo_put(drfifo_dev_t* drfifo, const void* data, size_t size)
{
    ssize_t bytes_put = 0;

    if ((NULL != drfifo) && (NULL != data))
    {
        KIRQL level;
//      DbgPrint(DRIVER_NAME ": drfifo_put() calling fifo_put(size=%d).", size);
        KeAcquireSpinLock(&drfifo->lock, &level);
        bytes_put = fifo_put(drfifo->fifo, data, size);
        KeReleaseSpinLock(&drfifo->lock, level);
//      DbgPrint(DRIVER_NAME ": drfifo_put() bytes_put=%d.", size);
    }

    return bytes_put;
}   /* drfifo_put() */

/* ------------------------------------------------------------------------- */
/**
 * Provides a protected FIFO get operation for the @a drfifo device
 * (extension).
 *
 * @param drfifo - device of interest.
 * @param data - pointer to buffer to hold data from the FIFO.
 * @param size - maximum number of bytes to get from the FIFO.
 *
 * @return the actual number of bytes read from the FIFO.
 */
static ssize_t drfifo_get(drfifo_dev_t* drfifo, void* data, size_t size)
{
    ssize_t bytes_gotten = 0;

    if ((NULL != drfifo) && (NULL != data))
    {
        KIRQL level;
//      DbgPrint(DRIVER_NAME ": drfifo_get() calling fifo_get(size=%d).", size);
        KeAcquireSpinLock(&drfifo->lock, &level);
        bytes_gotten = fifo_get(drfifo->fifo, data, size);
        KeReleaseSpinLock(&drfifo->lock, level);
//      DbgPrint(DRIVER_NAME ": drfifo_get() bytes_gotten=%d.", bytes_gotten);
    }

    return bytes_gotten;
}   /* drfifo_get() */

/* ------------------------------------------------------------------------- */
/**
 * Sets IRP major function @a irp_num to be handled by @a handler.
 * @param drv - device driver whose IRP dispatch table is to be changed.
 * @param irp_num - the IRP major function, 0 .. IRP_MJ_MAXIMUM_FUNCTION.
 * @param handler - pointer to function to use for all IRP major functions.
 */
static void irp_handler_set(PDRIVER_OBJECT drv, uint_t irp_num, PDRIVER_DISPATCH handler)
{
    if (irp_num <= IRP_MJ_MAXIMUM_FUNCTION)
    {
        drv->MajorFunction[irp_num] = handler;
    }
}   /* irp_handler_set() */

/* ------------------------------------------------------------------------- */
/**
 * Sets all major IRP functions to be handled by @a handler.
 * @param drv - device driver whose IRP dispatch table is to be changed.
 * @param handler - pointer to function to use for all IRP major functions.
 */
static void irp_handler_set_default(PDRIVER_OBJECT drv, PDRIVER_DISPATCH handler)
{
    uint_t irp_num =0;

    for (irp_num = 0; irp_num <= IRP_MJ_MAXIMUM_FUNCTION; irp_num++)
    {
        drv->MajorFunction[irp_num] = handler;
    }
}   /* irp_handler_set_default() */

/* ------------------------------------------------------------------------- */
/**
 * Calls IoCompleteRequest(@a irp, IO_NO_INCREMENT) after setting the
 * @a status to the given value.
 *
 * @param irp - request to complete.
 * @param info_bytes - number of bytes to indicate in the Information field.
 * @param status - result of transaction.
 *
 * @return @a status.
 */
static NTSTATUS irp_complete_event(PIRP irp, ULONG info_bytes, NTSTATUS status)
{
    if (NULL != irp)
    {
        irp->IoStatus.Information = info_bytes;
        irp->IoStatus.Status = status;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }

    return status;
}   /* irp_complete_event() */

/* ------------------------------------------------------------------------- */
/**
 * Default handler for an IRP. Does nothing; returns STATUS_SUCCESS.
 *
 * @param drv - device object.
 * @param irp - IRP requested.
 *
 * @return STATUS_SUCCESS on success, something else otherwise.
 */
NTSTATUS drfifo_handle_irp_default(IN PDEVICE_OBJECT dev, IN PIRP irp)
{
    PIO_STACK_LOCATION irp_stack;
//  PAGED_CODE();
    irp_stack = IoGetCurrentIrpStackLocation(irp);
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_default(0x%08X).", irp_stack->MajorFunction);
    return irp_complete_event(irp, 0, STATUS_SUCCESS);
}   /* drfifo_handle_irp_default() */

/* ------------------------------------------------------------------------- */
/**
 * Handles a create request IRP (IRP_MJ_CREATE).
 *
 * @param drv - device object.
 * @param irp - IRP requested.
 *
 * @return STATUS_SUCCESS on success, something else otherwise.
 */
NTSTATUS drfifo_handle_irp_create(IN PDEVICE_OBJECT dev, IN PIRP irp)
{
//  PAGED_CODE();
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_create().");
    return irp_complete_event(irp, 0, STATUS_SUCCESS);
}   /* drfifo_handle_irp_create() */

/* ------------------------------------------------------------------------- */
/**
 * Handles a read request IRP (IRP_MJ_READ).
 *
 * @param drv - device object.
 * @param irp - IRP requested.
 *
 * @return STATUS_SUCCESS on success, something else otherwise.
 */
NTSTATUS drfifo_handle_irp_read(IN PDEVICE_OBJECT dev, IN PIRP irp)
{
    PIO_STACK_LOCATION irp_stack = NULL;
    PVOID              ibuf = NULL;
    ULONG              ibuf_len = 0;
    ULONG              info_bytes = 0;
    drfifo_dev_t*      drfifo = (drfifo_dev_t*) dev->DeviceExtension;

//  PAGED_CODE();
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_read().");

    irp_stack = IoGetCurrentIrpStackLocation(irp);
    ibuf     = irp->AssociatedIrp.SystemBuffer;
    ibuf_len = irp_stack->Parameters.DeviceIoControl.OutputBufferLength;
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_read(ibuf=0x%08lX,ibuf_len=%d).", (long) ibuf, ibuf_len);

    if (ibuf_len > 0)
    {
        __try {
//          ProbeForWrite(ibuf, ibuf_len, 1);   // Not necessary for DO_BUFFERED_IO.
            DbgPrint(DRIVER_NAME ": drfifo_handle_irp_read() getting %d bytes.", ibuf_len);
            info_bytes = drfifo_get(drfifo, ibuf, ibuf_len);
            DbgPrint(DRIVER_NAME ": drfifo_handle_irp_read() info_bytes=%d.", info_bytes);
        }
        __except(1) {
            DbgPrint(DRIVER_NAME ": drfifo_handle_irp_read() SEGFAULT.");
            return irp_complete_event(irp, 0, STATUS_INVALID_ADDRESS);
        }
    }

    return irp_complete_event(irp, info_bytes, STATUS_SUCCESS);
}   /* drfifo_handle_irp_read() */

/* ------------------------------------------------------------------------- */
/**
 * Handles a write request IRP (IRP_MJ_WRITE).
 *
 * @param drv - device object.
 * @param irp - IRP requested.
 *
 * @return STATUS_SUCCESS on success, something else otherwise.
 */
NTSTATUS drfifo_handle_irp_write(IN PDEVICE_OBJECT dev, IN PIRP irp)
{
    PIO_STACK_LOCATION irp_stack = NULL;
    PVOID              obuf = NULL;
    ULONG              obuf_len = 0;
    ULONG              info_bytes = 0;
    drfifo_dev_t*      drfifo = (drfifo_dev_t*) dev->DeviceExtension;

//  PAGED_CODE();
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_write().");

    irp_stack = IoGetCurrentIrpStackLocation(irp);
    obuf     = irp->AssociatedIrp.SystemBuffer;
    obuf_len = irp_stack->Parameters.DeviceIoControl.OutputBufferLength;
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_write(obuf=0x%08lX,obuf_len=%d).", (unsigned long) obuf, obuf_len);

    if (obuf_len > 0)
    {
        if (obuf_len > fifo_bytes_to_put(drfifo->fifo))
        {
            DbgPrint(DRIVER_NAME ": drfifo_handle_irp_write() no room in FIFO.");
            return irp_complete_event(irp, 0, STATUS_INSUFFICIENT_RESOURCES);
        }

        __try {
//          ProbeForRead(obuf, obuf_len, 1);     // Not necessary - and fails! - for DO_BUFFERED_IO.
//          DbgPrint(DRIVER_NAME ": drfifo_handle_irp_write() putting %d bytes; %d available.",
//                   obuf_len, fifo_bytes_to_put(drfifo->fifo));
            info_bytes = drfifo_put(drfifo, obuf, obuf_len);
//          DbgPrint(DRIVER_NAME ": drfifo_handle_irp_write() info_bytes=%d.", info_bytes);
        }
        __except(1) {
            DbgPrint(DRIVER_NAME ": drfifo_handle_irp_write() SEGFAULT.");
            return irp_complete_event(irp, 0, STATUS_INVALID_ADDRESS);
        }
    }

    return irp_complete_event(irp, info_bytes, STATUS_SUCCESS);
}   /* drfifo_handle_irp_write() */

/* ------------------------------------------------------------------------- */
/**
 * Handles an I/O control request IRP (IRP_MJ_DEVICE_CONTROL).
 *
 * @param drv - device object.
 * @param irp - IRP requested.
 *
 * @return STATUS_SUCCESS on success, something else otherwise.
 */
NTSTATUS drfifo_handle_irp_ioctl(IN PDEVICE_OBJECT dev, IN PIRP irp)
{
    NTSTATUS           result = STATUS_SUCCESS;
    PIO_STACK_LOCATION irp_stack;
    PVOID              ibuf = NULL;
    PVOID              obuf = NULL;
    ULONG              ibuf_len = 0;
    ULONG              obuf_len = 0;
    ULONG              command;
    ULONG              info_bytes = 0;
    drfifo_dev_t*      drfifo = (drfifo_dev_t*) dev->DeviceExtension;
    KIRQL              level;

//  PAGED_CODE();
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_ioctl().");

    // Get a pointer to the current location in the Irp. This is where
    // the function codes and parameters are located.
    irp_stack = IoGetCurrentIrpStackLocation(irp);

    if (IRP_MJ_DEVICE_CONTROL != irp_stack->MajorFunction)
    {
        DbgPrint(DRIVER_NAME ": ioctl(): invalid major function %d; should be IRP_MJ_DEVICE_CONTROL (%d).",
                 (int) irp_stack->MajorFunction, (int) IRP_MJ_DEVICE_CONTROL);
        return irp_complete_event(irp, 0, STATUS_INVALID_DEVICE_REQUEST);
    }

    result = STATUS_SUCCESS;

    ibuf      = irp->AssociatedIrp.SystemBuffer;
    ibuf_len  = irp_stack->Parameters.DeviceIoControl.InputBufferLength;
    obuf      = irp->AssociatedIrp.SystemBuffer;
    obuf_len  = irp_stack->Parameters.DeviceIoControl.OutputBufferLength;
    command   = irp_stack->Parameters.DeviceIoControl.IoControlCode;

    if (IRP_MJ_DEVICE_CONTROL != irp_stack->MajorFunction)
    {
        DbgPrint(DRIVER_NAME ": ioctl(): invalid major function %d; should be IRP_MJ_DEVICE_CONTROL (%d).",
                 (int) irp_stack->MajorFunction, (int) IRP_MJ_DEVICE_CONTROL);
        return irp_complete_event(irp, 0, STATUS_INVALID_DEVICE_REQUEST);
    }

    DbgPrint(DRIVER_NAME ": ioctl(command=0x%08lX).", command);

    switch (command)
    {
    case DRFIFO_IOCTL_RESET:
        if (NULL == drfifo->fifo)
        {
            DbgPrint(DRIVER_NAME ": ioctl(RESET) drfifo->fifo == NULL.");
            result = STATUS_DEVICE_NOT_READY;   // This is meant for removable disk drives (CDROMs), but I'll take it.
        }
        else
        {
            DbgPrint(DRIVER_NAME ": ioctl(RESET) setting get_count %d and put_count %d to 0.",
                     drfifo->fifo->get_count, drfifo->fifo->put_count);
            KeAcquireSpinLock(&drfifo->lock, &level);
            fifo_reset(drfifo->fifo);
            KeReleaseSpinLock(&drfifo->lock, level);
        }
        break;

    case DRFIFO_IOCTL_FLUSH:
        if (NULL == drfifo->fifo)
        {
            DbgPrint(DRIVER_NAME ": ioctl(FLUSH) drfifo->fifo == NULL.");
            result = STATUS_DEVICE_NOT_READY;
        }
        else
        {
            DbgPrint(DRIVER_NAME ": ioctl(FLUSH) setting get_count %d to put_count %d.",
                     drfifo->fifo->get_count, drfifo->fifo->put_count);
            KeAcquireSpinLock(&drfifo->lock, &level);
            drfifo->fifo->get_count = drfifo->fifo->put_count;
            KeReleaseSpinLock(&drfifo->lock, level);
        }
        break;

    case DRFIFO_IOCTL_STATUS:
        if (obuf_len < sizeof(drfifo_ioctl_status_t))
        {
            DbgPrint(DRIVER_NAME ": ioctl(STATUS) output buffer length too small (%d < %d).",
                     obuf_len, sizeof(drfifo_ioctl_status_t));
            result = STATUS_INVALID_DEVICE_REQUEST;   // STATUS_INFO_LENGTH_MISMATCH isn't quite right.
        }
        else if (NULL == drfifo->fifo)
        {
            DbgPrint(DRIVER_NAME ": ioctl(STATUS) drfifo->fifo == NULL.");
            result = STATUS_DEVICE_NOT_READY;
        }
        else
        {
            drfifo_ioctl_status_t* status = (drfifo_ioctl_status_t*) obuf;
            DbgPrint(DRIVER_NAME ": ioctl(STATUS) getting status.");
            KeAcquireSpinLock(&drfifo->lock, &level);
            status->size  = drfifo->fifo->size;
            status->flags = drfifo->fifo->flags;
            status->put_count = drfifo->fifo->put_count;
            status->get_count = drfifo->fifo->get_count;
            KeReleaseSpinLock(&drfifo->lock, level);
            info_bytes = sizeof(drfifo_ioctl_status_t);
        }

        break;

    default:
        DbgPrint(DRIVER_NAME ": ioctl() invalid command 0x%08lX.", command);
        result = STATUS_INVALID_DEVICE_REQUEST;
    }   // switch on command

    return irp_complete_event(irp, info_bytes, result);
}   /* drfifo_handle_irp_ioctl() */

/* ------------------------------------------------------------------------- */
/**
 * Handles a close request IRP (IRP_MJ_CLOSE).
 *
 * @param drv - device object.
 * @param irp - IRP requested.
 *
 * @return STATUS_SUCCESS on success, something else otherwise.
 */
NTSTATUS drfifo_handle_irp_close(IN PDEVICE_OBJECT dev, IN PIRP irp)
{
//  PAGED_CODE();
    DbgPrint(DRIVER_NAME ": drfifo_handle_irp_close()\r\n");
    return irp_complete_event(irp, 0, STATUS_SUCCESS);
}   /* drfifo_handle_irp_close() */

/* ------------------------------------------------------------------------- */
/**
 * Perform cleanup for our device driver @a drv when it is being removed from
 * the system.
 *
 * @param drv - device driver object pointer.
 *
 * @return STATUS_SUCCESS; this always succeeds.
 */
NTSTATUS drfifo_driver_unload(IN PDRIVER_OBJECT drv)
{
    UNICODE_STRING device_link_unicode;
    drfifo_dev_t*  drfifo;
    KIRQL          level;
//  PAGED_CODE();
    DbgPrint(DRIVER_NAME ": Unloading driver.\r\n");

    // Remove the device link before the device itself is removed.
    RtlInitUnicodeString(&device_link_unicode, DRFIFO_DEVICE_LINK);
    IoDeleteSymbolicLink(&device_link_unicode);

    if (NULL == g_dev)
    {
        DbgPrint(DRIVER_NAME ": wtf? g_dev == NULL!\r\n");
    }
    else
    {
        drfifo = (drfifo_dev_t*) g_dev->DeviceExtension;

        if (NULL == drfifo)
        {
            DbgPrint(DRIVER_NAME ": wtf? drfifo == NULL!\r\n");
        }
        else
        {
            KeAcquireSpinLock(&drfifo->lock, &level);
            fifo_del(&drfifo->fifo);
            KeReleaseSpinLock(&drfifo->lock, level);
        }

        IoDeleteDevice(g_dev);
    }

    return STATUS_SUCCESS;
}   /* drfifo_driver_unload() */

/* ------------------------------------------------------------------------- */
/**
 * Schedules an IO callback to write the date/time to a file with the given
 * @a filename.
 *
 * See:
 * - http://stackoverflow.com/questions/850530/kernel-mode-driver-write-to-file
 * - http://support.microsoft.com/kb/891805
 * - http://msdn.microsoft.com/en-us/library/windows/hardware/ff566380(v=vs.85).aspx
 *
 * @param filename - description
 */
IO_WORKITEM_ROUTINE append_time_to_file;
VOID append_time_to_file(PDEVICE_OBJECT DeviceObject,
                         PVOID          Context)
{
    UNICODE_STRING    uname;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS          status = STATUS_SUCCESS;
    HANDLE            handle = NULL;
    IO_STATUS_BLOCK   io_status;
    drfifo_dev_t*     drfifo = (drfifo_dev_t*) DeviceObject->DeviceExtension;

    DbgPrint("%s: Writing local time to \"C:\\drfifo.txt\".\r\n", DRIVER_NAME);

    IoFreeWorkItem(drfifo->work_item);
    drfifo->work_item = NULL;

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        DbgPrint("%s: At IRQL=%u instead of PASSIVE_LEVEL!.\r\n", DRIVER_NAME, (unsigned) KeGetCurrentIrql());
        return;
    }

    RtlInitUnicodeString(&uname, L"\\DosDevices\\C:\\drfifo.txt");
    InitializeObjectAttributes(&attr, &uname, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwCreateFile(&handle,
                          FILE_APPEND_DATA,   // GENERIC_WRITE
                          &attr,
                          &io_status,
                          NULL,
                          FILE_ATTRIBUTE_NORMAL,
                          0,
                          FILE_OPEN_IF,
                          FILE_SYNCHRONOUS_IO_NONALERT,
                          NULL,
                          0);

    if (NT_SUCCESS(status))
    {
        char buf[0x40] = "";
        LARGE_INTEGER now;
        LARGE_INTEGER loc;
        TIME_FIELDS tim;
        KeQuerySystemTime(&now);
        ExSystemTimeToLocalTime(&now, &loc);
        RtlTimeToTimeFields(&loc, &tim);
        RtlStringCchPrintfA(buf, sizeof(buf)-1, "%04u-%02u-%02u %02u:%02u:%02u\r\n",
                            (unsigned) tim.Year, (unsigned) tim.Month,  (unsigned) tim.Day,
                            (unsigned) tim.Hour, (unsigned) tim.Minute, (unsigned) tim.Second);
        buf[sizeof(buf)-1] = 0;
        ZwWriteFile(handle, NULL, NULL, NULL, &io_status, (PVOID) buf, strlen(buf), NULL, NULL);
        ZwClose(handle);
    }
    else
    {
        DbgPrint("%ws: ZwCreateFile() failed with 0x%08X.\n", (unsigned) status);
    }
}   /* append_time_to_file() */

/* ------------------------------------------------------------------------- */
/**
 * Entry point for driver.
 *
 * @param drv - a fresh driver object to be configured.
 *
 * @param registry_path - registry path in HKLM for this driver and its
 * associated device.
 *
 * @return STATUS_SUCCESS on success, something else on failure.
 */
NTSTATUS DriverEntry(IN PDRIVER_OBJECT drv, IN PUNICODE_STRING registry_path)
{
    UNICODE_STRING device_name_unicode;
    UNICODE_STRING device_link_unicode;
    NTSTATUS result = 0;
    drfifo_dev_t* drfifo = NULL;

    DbgPrint(DRIVER_NAME ": Loading driver.\r\n");

    drv->DriverUnload = drfifo_driver_unload;

    irp_handler_set_default(drv, drfifo_handle_irp_default);
    irp_handler_set(drv, IRP_MJ_CREATE,          drfifo_handle_irp_create);
    irp_handler_set(drv, IRP_MJ_READ,            drfifo_handle_irp_read);
    irp_handler_set(drv, IRP_MJ_WRITE,           drfifo_handle_irp_write);
    irp_handler_set(drv, IRP_MJ_DEVICE_CONTROL,  drfifo_handle_irp_ioctl);
    irp_handler_set(drv, IRP_MJ_CLOSE,           drfifo_handle_irp_close);

    RtlInitUnicodeString(&device_name_unicode, DRFIFO_DEVICE_NAME);
    RtlInitUnicodeString(&device_link_unicode, DRFIFO_DEVICE_LINK);

    /*
     * Now create a singleton device.
     *
     * Eventually I want to create a separate device with each CREATE major
     * function invocation so I can have multiple FIFOs. For now I create
     * just a single device when the driver is installed.
     */
#define OPEN_ONLY_BY_ADMIN 1
//#undef OPEN_ONLY_BY_ADMIN
#if defined(OPEN_ONLY_BY_ADMIN)
    result = IoCreateDeviceSecure(drv,
                                  sizeof(drfifo_dev_t),            // For driver extension - device-specific data (FDO).
                                  &device_name_unicode,
                                  FILE_DEVICE_DRFIFO,    // Used in ioctl() command values, but how else?
                                  0,
                                  FALSE,
                                  &SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
                                  &guid_drfifo,
                                  &g_dev);
#else
    result = IoCreateDevice(drv,
                            sizeof(drfifo_dev_t),            // For driver extension - device-specific data (FDO).
                            &device_name_unicode,
                            0, //     FILE_DEVICE_DRFIFO,    // Used in ioctl() command values, but how else?
                            0,
                            TRUE, // FALSE,    // Exclusive.
                            &g_dev);
#endif

    if (!NT_SUCCESS(result))
    {
        DbgPrint("%s: IoCreateDevice[Secure]() failed.\r\n", DRIVER_NAME);
        return result;
    }

    DbgPrint("%s: g_dev=0x%08LLX  0x%08LLX=drv->DeviceObject.\r\n", DRIVER_NAME,
             (uint64_t) g_dev, (uint64_t) drv->DeviceObject);
    // This makes any I/O buffer appear in irp->AssociatedIrp.SystemBuffer.
    g_dev->Flags |= DO_BUFFERED_IO;

    result = IoCreateSymbolicLink(&device_link_unicode, &device_name_unicode);

    if (!NT_SUCCESS(result))
    {
        DbgPrint("%s: IoCreateSymbolicLink() failed.\r\n", DRIVER_NAME);
        IoDeleteDevice(drv->DeviceObject);
//      drv->DeviceObject = NULL;    // Is this necessary? It was not in mktools.
        g_dev = NULL;      // Ditto.
        return result;
    }

    drfifo = (drfifo_dev_t*) g_dev->DeviceExtension;

    if (NULL == drfifo)
    {
        DbgPrint("%s: DeviceExtension is NULL!\r\n", DRIVER_NAME);
        IoDeleteDevice(drv->DeviceObject);
//      drv->DeviceObject = NULL;    // Is this necessary? It was not in mktools.
        g_dev = NULL;      // Ditto.
        return STATUS_UNSUCCESSFUL;
    }

    KeInitializeSpinLock(&drfifo->lock);
    drfifo->fifo = fifo_new(0x0800);
    fifo_packetized(drfifo->fifo, 1);
//  fifo_all_or_nothing_set(drfifo->fifo, 1);

    {
        drfifo->work_item = IoAllocateWorkItem(g_dev);

        if (NULL == drfifo->work_item)
        {
            DbgPrint("%s: Could not allocate work item for writing to file.\r\n", DRIVER_NAME);
        }
        else
        {
            IoQueueWorkItem(drfifo->work_item, append_time_to_file, DelayedWorkQueue, NULL);
        }
    }

    return STATUS_SUCCESS;
}   /* DriverEntry() */
