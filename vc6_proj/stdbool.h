#ifndef _MSC_VER // [
#error "Use this header only with Microsoft Visual C++ compilers!"
#endif // _MSC_VER ]

#ifndef _MSC_STDBOOL_H_ // [
#define _MSC_STDBOOL_H_

#if _MSC_VER > 1000
#pragma once
#endif

enum bool_t {
	false = 0,
	true  = 1,
};
typedef enum bool_t bool;

#endif
