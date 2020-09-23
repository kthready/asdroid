#ifndef ASSISTANT_COMMON_H
#define ASSISTANT_COMMON_H

#include <types.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

#define MAX_ERRNO	4095

#define IS_ERR_VALUE(x) ((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

#define ERR_PTR(error) ((void *)error)

#define PTR_ERR(ptr) ((long)ptr)

#define IS_ERR(ptr) (IS_ERR_VALUE((unsigned long)ptr))

#endif
