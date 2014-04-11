#ifndef CORE_TYPES_H_
#define CORE_TYPES_H_

#include <stdbool.h>


#define _GNU_SOURCE

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif



typedef unsigned char puint8;
typedef unsigned short puint16;
typedef unsigned long puint32;
typedef unsigned long long puint64;

typedef char pint8;
typedef short pint16;
typedef long pint32;
typedef long long pint64;


#endif
