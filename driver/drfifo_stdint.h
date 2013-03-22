#ifndef __drfifo_stdint_h__
#define __drfifo_stdint_h__

/*
 * Shorter names for C types.
 */
typedef char                schar_t;
typedef unsigned char       uchar_t;
typedef short               sshort_t;
typedef unsigned short      ushort_t;
typedef int                 sint_t;
typedef unsigned int        uint_t;
typedef long                slong_t;
typedef unsigned long       ulong_t;
typedef long long           slonglong_t;
typedef unsigned long long  ulonglong_t;

/*
 * Map them to fixed-sized types. This really should be wrapped in #ifdef's
 * for various development environments.
 */
typedef schar_t      int8_t;
typedef uchar_t      uint8_t;
typedef sshort_t     int16_t;
typedef ushort_t     uint16_t;
typedef slong_t      int32_t;
typedef ulong_t      uint32_t;
typedef slonglong_t  int64_t;
typedef ulonglong_t  uint64_t;

#endif
