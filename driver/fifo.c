#include <stdlib.h>

#if defined(WINDDK) || defined(NT_INST)
#include <ntddk.h>
#include <wdm.h>
//#include <wdmsec.h>
#define fifo_mem_alloc(_size)               MmAllocateNonCachedMemory(_size)
#define fifo_mem_copy_into(_dst,_src,_len)  RtlCopyMemory(_dst, _src, _len)
#define fifo_mem_copy_from(_dst,_src,_len)  RtlCopyMemory(_dst, _src, _len)
#define fifo_mem_free(_ptr,_size)           MmFreeNonCachedMemory(_ptr, _size)
#else    // standard C in user land...
#error Not DDK.
#define fifo_mem_alloc(_size)               malloc(_size)
#define fifo_mem_copy_into(_dst,_src,_len)  memcpy(_dst, _src, _len)
#define fifo_mem_copy_from(_dst,_src,_len)  memcpy(_dst, _src, _len)
#define fifo_mem_free(_ptr,_size)           free(_ptr)
#endif

#include "fifo.h"

/**
 * Flag to enable all-or-nothing operations.
 */
#define FIFO_FLAG_ALL_OR_NOTHING   (1 << 0)

/**
 * Flag to enable packetized operations. Each get() will read the same amount
 * that the associated put() wrote. If there's not enough room in the
 * reader's buffer then it will receive the truncated packet data.
 *
 * Packetized data result in the insertion of a length before the data are
 * written.
 */
#define FIFO_FLAG_PACKETIZED       (1 << 1)

/* ------------------------------------------------------------------------- */
/**
 * Allocates a fifo struct and the associated data.
 */
fifo_t* fifo_new(size_t bytes)
{
    fifo_t* fifo = fifo_mem_alloc(sizeof(fifo_t) + bytes);

    if (NULL != fifo)
    {
        memset(fifo, 0, sizeof(fifo_t));
        fifo->size = bytes;
    }

    return fifo;
}   /* fifo_new() */

/* ------------------------------------------------------------------------- */
/**
 * Deletes a fifo object, NULL-ing the pointer.
 */
void fifo_del(fifo_t** fifo_ptr)
{
    if ((NULL != fifo_ptr) && (NULL != *fifo_ptr))
    {
        fifo_t* fifo = *fifo_ptr;
        *fifo_ptr = NULL;
        fifo_mem_free(fifo, sizeof(fifo_t) + fifo->size);
    }
}   /* fifo_del() */

/* ------------------------------------------------------------------------- */
/**
 * Resets the @a fifo's counters to 0, but does *not* modify its modes of
 * operation.
 */
void fifo_reset(fifo_t* fifo)
{
    if (NULL != fifo)
    {
        fifo->get_count = 0;
        fifo->put_count = 0;
    }
}   /* fifo_reset() */

/* ------------------------------------------------------------------------- */
int8_t fifo_is_all_or_nothing(const fifo_t* fifo)
{
    return (NULL == fifo) ? 0 : ((fifo->flags & FIFO_FLAG_ALL_OR_NOTHING) != 0);
}   /* fifo_is_all_or_nothing() */

/* ------------------------------------------------------------------------- */
int8_t fifo_all_or_nothing(fifo_t* fifo, int8_t enabled)
{
    int8_t result = fifo_is_all_or_nothing(fifo);

    if (NULL != fifo)
    {
        if (enabled)
        {
            fifo->flags |=  FIFO_FLAG_ALL_OR_NOTHING;
        }
        else
        {
            fifo->flags &= ~FIFO_FLAG_ALL_OR_NOTHING;
        }
    }

    return result;
}   /* fifo_all_or_nothing() */

/* ------------------------------------------------------------------------- */
int8_t fifo_is_packetized(const fifo_t* fifo)
{
    return (NULL == fifo) ? 0 : ((fifo->flags & FIFO_FLAG_PACKETIZED) != 0);
}   /* fifo_is_packetized() */

/* ------------------------------------------------------------------------- */
/**
 * Enables or disables packetized transactions in the FIFO.
 *
 * @note Since this changes the way that data are stored in the FIFO, this
 * call will *always* reset the FIFO via fifo_reset().
 */
int8_t fifo_packetized(fifo_t* fifo, int8_t enabled)
{
    int8_t result = fifo_is_packetized(fifo);

    if (NULL != fifo)
    {
        if (enabled)
        {
            fifo->flags |=  FIFO_FLAG_PACKETIZED;
        }
        else
        {
            fifo->flags &= ~FIFO_FLAG_PACKETIZED;
        }

        fifo_reset(fifo);
    }

    return result;
}   /* fifo_packetized() */

/* ------------------------------------------------------------------------- */
/**
 * Copies @a bytes from @a data into the @a fifo; no checking is performed
 * but the put_count field is incremented.
 */
static void prechecked_fifo_raw_put(fifo_t* fifo, const void* data, size_t bytes)
{
    const uint8_t* src = (const uint8_t*) data;
    const size_t put_index = fifo->put_count % fifo->size;
    const size_t bytes_to_end = fifo->size - put_index;

    DbgPrint("prechecked_fifo_raw_put(%u) at [%u], bytes_to_end=%u.\r\n", bytes, put_index, bytes_to_end);

    if (bytes <= bytes_to_end)
    {
        fifo_mem_copy_into(&fifo->data[put_index], src, bytes);
    }
    else
    {
        fifo_mem_copy_into(&fifo->data[put_index], src, bytes_to_end);
        fifo_mem_copy_into(&fifo->data[0], &src[bytes_to_end], bytes - bytes_to_end);
    }

    fifo->put_count += bytes;
}   /* prechecked_fifo_raw_put() */

/* ------------------------------------------------------------------------- */
/**
 * Copies up to @a bytes bytes from @a data into the fifo.
 */
ssize_t fifo_put(fifo_t* fifo, const void* data, size_t bytes)
{
    const size_t bytes_available_to_put = fifo_bytes_to_put(fifo);

    if ((NULL == fifo) || (0 == bytes_available_to_put))
    {
        return 0;
    }

    if (bytes > bytes_available_to_put)
    {
        if (fifo_is_all_or_nothing(fifo))
        {
            return 0;
        }
        else
        {
            bytes = bytes_available_to_put;
        }
    }

    if (!fifo_is_packetized(fifo))
    {
        prechecked_fifo_raw_put(fifo, data, bytes);
    }
    else
    {
        const size_t size = bytes;
        // There's a minor race condition here over the value of put_count,
        // but this code is not guaranteed to be thread-safe.
        prechecked_fifo_raw_put(fifo, &size, sizeof(size_t));
        prechecked_fifo_raw_put(fifo, data, bytes);
    }

    return bytes;
}   /* fifo_put() */

/* ------------------------------------------------------------------------- */
/**
 * Copies @a bytes from the @a fifo into @a data; no checking is performed,
 * but the get_count field is incremented.
 */
static void prechecked_fifo_raw_get(fifo_t* fifo, void* data, size_t bytes)
{
    uint8_t* dst = (uint8_t*) data;
    const size_t get_index = fifo->get_count % fifo->size;
    const size_t bytes_to_end = fifo->size - get_index;

    DbgPrint("prechecked_fifo_raw_get(%u) at [%u], bytes_to_end=%u.\r\n", bytes, get_index, bytes_to_end);

    if (bytes <= bytes_to_end)
    {
        fifo_mem_copy_from(dst, &fifo->data[get_index], bytes);
    }
    else
    {
        fifo_mem_copy_from(dst, &fifo->data[get_index], bytes_to_end);
        fifo_mem_copy_from(&dst[bytes_to_end], &fifo->data[0], bytes - bytes_to_end);
    }

    fifo->get_count += bytes;
}   /* prechecked_fifo_raw_get() */

/* ------------------------------------------------------------------------- */
/**
 * Reads @a bytes bytes from the fifo into the @a data buffer.
 */
ssize_t fifo_get(fifo_t* fifo, void* data, size_t bytes)
{
    const size_t bytes_available_to_get = fifo_bytes_to_get(fifo);

    if ((NULL == fifo) || (0 == bytes_available_to_get))
    {
        return 0;
    }

    if (bytes > bytes_available_to_get)
    {
        if (fifo_is_all_or_nothing(fifo))
        {
            return 0;
        }
        else
        {
            bytes = bytes_available_to_get;
        }
    }

    if (!fifo_is_packetized(fifo))
    {
        prechecked_fifo_raw_get(fifo, data, bytes);
    }
    else
    {
        size_t packet_bytes = bytes;
        prechecked_fifo_raw_get(fifo, &packet_bytes, sizeof(size_t));

        if (packet_bytes > bytes_available_to_get)
        {
            DbgPrint("fifo_get() Internal error! %u > %u.\r\n", packet_bytes, bytes_available_to_get);
            // Internal error! This should never happen.
            fifo_reset(fifo);
            return 0;   // -----------------------------------> return!
        }

        if (packet_bytes < bytes)
        {
            bytes = packet_bytes;
        }

        prechecked_fifo_raw_get(fifo, data, bytes);
        fifo->get_count += packet_bytes - bytes;    // Skip forward to next packet.
    }

    return bytes;
}   /* fifo_get() */

/* ------------------------------------------------------------------------- */
/**
 * @return the number of bytes available to be put into @a fifo.
 */
size_t fifo_bytes_to_put(const fifo_t* fifo)
{
    size_t bytes = 0;

    if (NULL != fifo)
    {
        bytes = fifo->size - (fifo->put_count - fifo->get_count);

        if (fifo_is_packetized(fifo))
        {
            if (bytes <= sizeof(size_t))
            {
                bytes = 0;
            }
            else
            {
                bytes -= sizeof(size_t);
            }
        }
    }

    return bytes;
}   /* fifo_bytes_to_put() */

/* ------------------------------------------------------------------------- */
/**
 * @return the number of bytes available to be gotten from @a fifo. When in
 * packet mode this may reflect more than are available on the next read.
 */
size_t fifo_bytes_to_get(const fifo_t* fifo)
{
    size_t bytes = 0;

    if (NULL != fifo)
    {
        bytes = fifo->put_count - fifo->get_count;

        if (fifo_is_packetized(fifo))
        {
            if (bytes <= sizeof(size_t))
            {
                bytes = 0;
            }
            else
            {
                bytes -= sizeof(size_t);
            }
        }
    }

    return bytes;
}   /* fifo_bytes_to_get() */

