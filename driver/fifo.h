#ifndef __fifo_h__
#define __fifo_h__

#include "drfifo_stdint.h"

typedef struct fifo_s fifo_t;

typedef int          ssize_t;
//typedef unsigned int size_t;

typedef uint_t fifo_flags_t;

/**
 * Main FIFO structure. This should be considered private but is provided for
 * static allocation and status introspection.
 */
struct fifo_s
{
    size_t   size;       /**< Number of data bytes in the buffer. */
    size_t   flags;      /**< Flags for this FIFO; used internally. */
    size_t   put_count;  /**< Number of bytes written to the FIFO. */
    size_t   get_count;  /**< Number of bytes read from the FIFO. */
    uint8_t  data[0];    /**< FIFO data. */
};   /* struct fifo_s */

/**
 * Structure used with fifo_scatter_get() to get a list of buffers from the
 * fifo.
 */
typedef struct fifo_get_data_s
{
    void*  data;
    size_t size;
} fifo_get_data_t;

/**
 * Structure used with fifo_scatter_put() to put a list of buffers into the
 * fifo. This is basically a const version of fifo_get_data_t.
 */
typedef struct fifo_put_data_s
{
    const void* data;
    size_t      size;
} fifo_put_data_t;


//fifo_t* fifo_init(fifo_t* fifo, void* data, size_t size);
//void    fifo_exit(fifo_t** fifo_ptr);

fifo_t* fifo_new(size_t bytes);
void fifo_del(fifo_t** fifo_ptr);

void   fifo_reset(fifo_t* fifo);

int8_t fifo_is_all_or_nothing(const fifo_t* fifo);
int8_t fifo_all_or_nothing(fifo_t* fifo, int8_t enabled);
int8_t fifo_is_packetized_get(const fifo_t* fifo);            // Each transaction is a packet; resets FIFO.
int8_t fifo_packetized(fifo_t* fifo, int8_t enabled);

ssize_t fifo_put(fifo_t* fifo, const void* data, size_t bytes);
ssize_t fifo_get(fifo_t* fifo,       void* data, size_t bytes);
//ssize_t fifo_scatter_put(fifo_t* fifo, const fifo_put_data_t list[], size_t count);
//ssize_t fifo_scatter_get(fifo_t* fifo, const fifo_get_data_t list[], size_t count);
size_t  fifo_bytes_to_put(const fifo_t* fifo);   // Removes sizeof(size_t) for packetized transactions.
size_t  fifo_bytes_to_get(const fifo_t* fifo);

#endif
