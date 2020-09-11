#ifndef ASSISTANT_COMMON_H
#define ASSISTANT_COMMON_H

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

#define MAX_ERRNO	4095

#define IS_ERR_VALUE(x) ((unsigned long)(void *)(x) >= (unsigned long)-MAX_ERRNO)

#define ERR_PTR(error) ((void *)error)

#define PTR_ERR(ptr) ((long)ptr)

#define IS_ERR(ptr) (IS_ERR_VALUE((unsigned long)ptr))

#endif
