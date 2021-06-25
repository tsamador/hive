#ifndef HIVE_TYPES_H
#define HIVE_TYPES_H
/*
    Author: Tanner Amador
    Date: 6/16/2021
    Contains type declarations and useful
    constants.
*/

#include <stdint.h>
#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

enum KEYCODE {
    KEYNULL,
    KEYUP,
    KEYDOWN,
    KEYLEFT,
    KEYRIGHT,
    KEYSPACE
};

struct keyboard_input {
    KEYCODE keycode;
    bool wasDown;
};

#endif