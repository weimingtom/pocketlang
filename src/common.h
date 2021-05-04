/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef MS_COMMON_H
#define MS_COMMON_H

#include "miniscript.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// miniscript visibility macros. define MS_DLL for using miniscript as a 
// shared library and define MS_COMPILE to export symbols.

#ifdef _MSC_VER
  #define _MS_EXPORT __declspec(dllexport)
  #define MS_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
  #define _MS_EXPORT __attribute__((visibility ("default")))
  #define _MS_IMPORT
#else
  #define _MS_EXPORT
  #define _MS_IMPORT
#endif

#ifdef MS_DLL
  #ifdef MS_COMPILE
    #define MS_PUBLIC _MS_EXPORT
  #else
    #define MS_PUBLIC _MS_IMPORT
  #endif
#else
  #define MS_PUBLIC
#endif

// Set this to dump compiled opcodes of each functions.
#define DEBUG_DUMP_COMPILED_CODE 0

#ifdef DEBUG

#include <stdio.h>

#ifdef _MSC_VER
  #define DEBUG_BREAK() __debugbreak()
#else
  #define DEBUG_BREAK() __builtin_trap()
#endif

#define ASSERT(condition, message)                                   \
  do {                                                               \
    if (!(condition)) {                                              \
      fprintf(stderr, "Assertion failed: %s\n\tat %s() (%s:%i)\n",   \
        message, __func__, __FILE__, __LINE__);                      \
      DEBUG_BREAK();                                                 \
      abort();                                                       \
    }                                                                \
  } while (false)

#define ASSERT_INDEX(index, size) \
  ASSERT(index >= 0 && index < size, "Index out of bounds.")

#define UNREACHABLE()                                                \
  do {                                                               \
    fprintf(stderr, "Execution reached an unreachable path\n"        \
      "\tat %s() (%s:%i)\n", __func__, __FILE__, __LINE__);          \
    abort();                                                         \
  } while (false)

#else

#define DEBUG_BREAK()
#define ASSERT(condition, message) do { } while (false)
#define ASSERT_INDEX(index, size) do {} while (false)

// Reference : https://github.com/wren-lang/
#if defined( _MSC_VER )
  #define UNREACHABLE() __assume(0)
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
  #define UNREACHABLE() __builtin_unreachable()
#else
  #define UNREACHABLE()
#endif

#endif // DEBUG

#define TODO ASSERT(false, "TODO")
#define OOPS "Oops a bug!! report plese."

#define STRINGIFY(x) TOSTRING(x)
#define TOSTRING(x) #x

// The factor by which a buffer will grow when it's capacity reached.
#define GROW_FACTOR 2

// The initial capacity of a buffer.
#define MIN_CAPACITY 8

// Allocate object of [type] using the vmRealloc function.
#define ALLOCATE(vm, type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type)))

// Allocate object of [type] which has a dynamic tail array of type [tail_type]
// with [count] entries.
#define ALLOCATE_DYNAMIC(vm, type, count, tail_type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type) + sizeof(tail_type) * (count)))

// Allocate [count] ammount of object of [type] array.
#define ALLOCATE_ARRAY(vm, type, count) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type) * (count)))

// Deallocate a pointer allocated by vmRealloc before.
#define DEALLOCATE(vm, pointer) \
    vmRealloc(vm, pointer, 0, 0)

typedef struct Object Object;
typedef struct String String;
typedef struct List List;
typedef struct Map Map;
typedef struct Range Range;

typedef struct Script Script;
typedef struct Function Function;

// Unique number to identify for various cases.
typedef uint32_t ID;

// VM's fiber type.
typedef struct Fiber Fiber;

#endif //MS_COMMON_H