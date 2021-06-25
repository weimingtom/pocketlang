/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "pk_core.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "pk_debug.h"
#include "pk_utils.h"
#include "pk_var.h"
#include "pk_vm.h"

#if defined(_MSC_VER) && _MSC_VER <= 1200
#define inline __inline
#endif

// M_PI is non standard. The macro _USE_MATH_DEFINES defining before importing
// <math.h> will define the constants for MSVC. But for a portable solution,
// we're defining it ourselves if it isn't already.
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// Returns the docstring of the function, which is a static const char* defined
// just above the function by the DEF() macro below.
#define DOCSTRING(fn) _pk_doc_##fn

// A macro to declare a function, with docstring, which is defined as
// _pk_doc_<fn> = docstring; That'll used to generate function help text.
#define DEF(fn, docstring)                      \
  static const char* DOCSTRING(fn) = docstring; \
  static void fn(PKVM* vm)

// Checks if a number is a byte
#define IS_NUM_BYTE(num) ((CHAR_MIN <= (num)) && ((num) <= CHAR_MAX))

/*****************************************************************************/
/* CORE PUBLIC API                                                           */
/*****************************************************************************/

// Create a new module with the given [name] and returns as a Script* for
// internal. Which will be wrapped by pkNewModule to return a pkHandle*.
static Script* newModuleInternal(PKVM* vm, const char* name);

// The internal function to add global value to a module.
static void moduleAddGlobalInternal(PKVM* vm, Script* script,
                                    const char* name, Var value);

// The internal function to add functions to a module.
static void moduleAddFunctionInternal(PKVM* vm, Script* script,
                                      const char* name, pkNativeFn fptr,
                                      int arity, const char* docstring);

// pkNewModule implementation (see pocketlang.h for description).
PkHandle* pkNewModule(PKVM* vm, const char* name) {
  Script* module = newModuleInternal(vm, name);
  return vmNewHandle(vm, VAR_OBJ(module));
}

// pkModuleAddGlobal implementation (see pocketlang.h for description).
PK_PUBLIC void pkModuleAddGlobal(PKVM* vm, PkHandle* module,
                                 const char* name, PkHandle* value) {
  Var scr;
  __ASSERT(module != NULL, "Argument module was NULL.");
  __ASSERT(value != NULL, "Argument value was NULL.");
  scr = module->value;
  __ASSERT(IS_OBJ_TYPE(scr, OBJ_SCRIPT), "Given handle is not a module");

  moduleAddGlobalInternal(vm, (Script*)AS_OBJ(scr), name, value->value);
}

// pkModuleAddFunction implementation (see pocketlang.h for description).
void pkModuleAddFunction(PKVM* vm, PkHandle* module, const char* name,
                         pkNativeFn fptr, int arity) {
  Var scr;
  __ASSERT(module != NULL, "Argument module was NULL.");
  scr = module->value;
  __ASSERT(IS_OBJ_TYPE(scr, OBJ_SCRIPT), "Given handle is not a module");
  moduleAddFunctionInternal(vm, (Script*)AS_OBJ(scr), name, fptr, arity,
                            NULL /*TODO: Public API for function docstring.*/);
}

PkHandle* pkGetFunction(PKVM* vm, PkHandle* module,
                                  const char* name) {
  Var scr;
  Script* script;
  uint32_t i;
  __ASSERT(module != NULL, "Argument module was NULL.");
  scr = module->value;
  __ASSERT(IS_OBJ_TYPE(scr, OBJ_SCRIPT), "Given handle is not a module");
  script = (Script*)AS_OBJ(scr);

  for (i = 0; i < script->functions.count; i++) {
    const char* fn_name = script->functions.data[i]->name;
    if (strcmp(name, fn_name) == 0) {
      return vmNewHandle(vm, VAR_OBJ(script->functions.data[i]));
    }
  }

  return NULL;
}

// A convenient macro to get the nth (1 based) argument of the current
// function.
#define ARG(n) (vm->fiber->ret[n])

// Evaluates to the current function's argument count.
#define ARGC ((int)(vm->fiber->sp - vm->fiber->ret) - 1)

// Set return value for the current native function and return.
#define RET(value)             \
  do {                         \
    *(vm->fiber->ret) = value; \
    return;                    \
  } while (false)

#define RET_ERR(err)           \
  do {                         \
    VM_SET_ERROR(vm, err);     \
    RET(VAR_NULL);             \
  } while(false)

// Check for errors in before calling the get arg public api function.
#define CHECK_GET_ARG_API_ERRORS()                                   \
  do {                                                               \
    __ASSERT(vm->fiber != NULL,                                      \
             "This function can only be called at runtime.");        \
    __ASSERT(arg > 0 || arg <= ARGC, "Invalid argument index.");     \
    __ASSERT(value != NULL, "Argument [value] was NULL.");           \
  } while (false)

// Set error for incompatible type provided as an argument. (TODO: got type).
#define ERR_INVALID_ARG_TYPE(m_type)                                 \
do {                                                                 \
  char buff[STR_INT_BUFF_SIZE];                                      \
  sprintf(buff, "%d", arg);                                          \
  VM_SET_ERROR(vm, stringFormat(vm, "Expected a '$' at argument $.", \
                                    m_type, buff));                  \
} while (false)

// pkGetArgc implementation (see pocketlang.h for description).
int pkGetArgc(const PKVM* vm) {
  __ASSERT(vm->fiber != NULL, "This function can only be called at runtime.");
  return ARGC;
}

// pkGetArg implementation (see pocketlang.h for description).
PkVar pkGetArg(const PKVM* vm, int arg) {
  __ASSERT(vm->fiber != NULL, "This function can only be called at runtime.");
  __ASSERT(arg > 0 || arg <= ARGC, "Invalid argument index.");

  return &(ARG(arg));
}

// pkGetArgBool implementation (see pocketlang.h for description).
bool pkGetArgBool(PKVM* vm, int arg, bool* value) {
  Var val;
  CHECK_GET_ARG_API_ERRORS();

  val = ARG(arg);
  *value = toBool(val);
  return true;
}

// pkGetArgNumber implementation (see pocketlang.h for description).
bool pkGetArgNumber(PKVM* vm, int arg, double* value) {
  Var val;
  CHECK_GET_ARG_API_ERRORS();

  val = ARG(arg);
  if (IS_NUM(val)) {
    *value = AS_NUM(val);

  } else if (IS_BOOL(val)) {
    *value = AS_BOOL(val) ? 1 : 0;

  } else {
    ERR_INVALID_ARG_TYPE("number");
    return false;
  }

  return true;
}

// pkGetArgString implementation (see pocketlang.h for description).
bool pkGetArgString(PKVM* vm, int arg, const char** value, uint32_t* length) {
  Var val;
  CHECK_GET_ARG_API_ERRORS();

  val = ARG(arg);
  if (IS_OBJ_TYPE(val, OBJ_STRING)) {
    String* str = (String*)AS_OBJ(val);
    *value = str->data;
    if (length) *length = str->length;

  } else {
    ERR_INVALID_ARG_TYPE("string");
    return false;
  }

  return true;
}

// pkGetArgInstance implementation (see pocketlang.h for description).
bool pkGetArgInst(PKVM* vm, int arg, uint32_t id, void** value) {
  Var val;
  bool is_native_instance;
  CHECK_GET_ARG_API_ERRORS();

  val = ARG(arg);
  is_native_instance = false;

  if (IS_OBJ_TYPE(val, OBJ_INST)) {
    Instance* inst = ((Instance*)AS_OBJ(val));
    if (inst->is_native && inst->native_id == id) {
      *value = inst->native;
      is_native_instance = true;
    }
  }

  if (!is_native_instance) {
    const char* ty_name = "$(?)";
    if (vm->config.inst_name_fn != NULL) {
      ty_name = vm->config.inst_name_fn(id);
    }

    ERR_INVALID_ARG_TYPE(ty_name);
    return false;
  }

  return true;
}

// pkGetArgValue implementation (see pocketlang.h for description).
bool pkGetArgValue(PKVM* vm, int arg, PkVarType type, PkVar* value) {
  Var val;
  CHECK_GET_ARG_API_ERRORS();

  val = ARG(arg);
  if (pkGetValueType((PkVar)&val) != type) {
    char buff[STR_INT_BUFF_SIZE]; sprintf(buff, "%d", arg);
    VM_SET_ERROR(vm, stringFormat(vm, "Expected a $ at argument $.",
                 getPkVarTypeName(type), buff));
    return false;
  }

  *value = (PkVar)&val;
  return true;
}

// pkReturnNull implementation (see pocketlang.h for description).
void pkReturnNull(PKVM* vm) {
  RET(VAR_NULL);
}

// pkReturnBool implementation (see pocketlang.h for description).
void pkReturnBool(PKVM* vm, bool value) {
  RET(VAR_BOOL(value));
}

// pkReturnNumber implementation (see pocketlang.h for description).
void pkReturnNumber(PKVM* vm, double value) {
  RET(VAR_NUM(value));
}

// pkReturnString implementation (see pocketlang.h for description).
void pkReturnString(PKVM* vm, const char* value) {
  RET(VAR_OBJ(newString(vm, value)));
}

// pkReturnStringLength implementation (see pocketlang.h for description).
void pkReturnStringLength(PKVM* vm, const char* value, size_t length) {
  RET(VAR_OBJ(newStringLength(vm, value, (uint32_t)length)));
}

// pkReturnValue implementation (see pocketlang.h for description).
void pkReturnValue(PKVM* vm, PkVar value) {
  RET(*(Var*)value);
}

// pkReturnHandle implementation (see pocketlang.h for description).
void pkReturnHandle(PKVM* vm, PkHandle* handle) {
  RET(handle->value);
}

// pkReturnInstNative implementation (see pocketlang.h for description).
void pkReturnInstNative(PKVM* vm, void* data, uint32_t id) {
  RET(VAR_OBJ(newInstanceNative(vm, data, id)));
}

const char* pkStringGetData(const PkVar value) {
  const Var str = (*(const Var*)value);
  __ASSERT(IS_OBJ_TYPE(str, OBJ_STRING), "Value should be of type string.");
  return ((String*)AS_OBJ(str))->data;
}

PkVar pkFiberGetReturnValue(const PkHandle* fiber) {
  Var fb;
  Fiber* _fiber;
  __ASSERT(fiber != NULL, "Handle fiber was NULL.");
  fb = fiber->value;
  __ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber");
  _fiber = (Fiber*)AS_OBJ(fb);
  return (PkVar)_fiber->ret;
}

bool pkFiberIsDone(const PkHandle* fiber) {
  Var fb;
  Fiber* _fiber;
  __ASSERT(fiber != NULL, "Handle fiber was NULL.");
  fb = fiber->value;
  __ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber");
  _fiber = (Fiber*)AS_OBJ(fb);
  return _fiber->state == FIBER_DONE;
}

/*****************************************************************************/
/* VALIDATORS                                                                */
/*****************************************************************************/

// Check if [var] is a numeric value (bool/number) and set [value].
static inline bool isNumeric(Var var, double* value) {
  if (IS_NUM(var)) {
    *value = AS_NUM(var);
    return true;
  }
  if (IS_BOOL(var)) {
    *value = AS_BOOL(var);
    return true;
  }
  return false;
}

// Check if [var] is a numeric value (bool/number) and set [value].
static inline bool isInteger(Var var, int64_t* value) {
  double number;
  if (isNumeric(var, &number)) {
    // TODO: check if the number is larger for a 64 bit integer.
    if (floor(number) == number) {
      ASSERT(INT64_MIN <= number && number <= INT64_MAX,
        "TODO: Large numbers haven't handled yet. Please report!");
      *value = (int64_t)(number);
      return true;
    }
  }
  return false;
}

// Check if [var] is bool/number. If not set error and return false.
static inline bool validateNumeric(PKVM* vm, Var var, double* value,
                                   const char* name) {
  if (isNumeric(var, value)) return true;
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be a numeric value.", name));
  return false;
}

// Check if [var] is 32 bit integer. If not set error and return false.
static inline bool validateInteger(PKVM* vm, Var var, int64_t* value,
                                   const char* name) {
  if (isInteger(var, value)) return true;
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be a whole number.", name));
  return false;
}

// Index is could be larger than 32 bit integer, but the size in pocketlang
// limited to 32 unsigned bit integer
static inline bool validateIndex(PKVM* vm, int64_t index, uint32_t size,
                                 const char* container) {
  if (index < 0 || size <= index) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ index out of bound.", container));
    return false;
  }
  return true;
}

// Check if [var] is string for argument at [arg]. If not set error and
// return false.
#define VALIDATE_ARG_OBJ(m_class, m_type, m_name)                            \
  static bool validateArg##m_class(PKVM* vm, int arg, m_class** value) {     \
    Var var = ARG(arg);                                                      \
    ASSERT(arg > 0 && arg <= ARGC, OOPS);                                    \
    if (!IS_OBJ(var) || AS_OBJ(var)->type != m_type) {                       \
      char buff[12]; sprintf(buff, "%d", arg);                               \
      VM_SET_ERROR(vm, stringFormat(vm, "Expected a " m_name                 \
                   " at argument $.", buff, false));                         \
    }                                                                        \
    *value = (m_class*)AS_OBJ(var);                                          \
    return true;                                                             \
   }
 VALIDATE_ARG_OBJ(String, OBJ_STRING, "string")
 VALIDATE_ARG_OBJ(List, OBJ_LIST, "list")
 VALIDATE_ARG_OBJ(Map, OBJ_MAP, "map")
 VALIDATE_ARG_OBJ(Function, OBJ_FUNC, "function")
 VALIDATE_ARG_OBJ(Fiber, OBJ_FIBER, "fiber")

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

// findBuiltinFunction implementation (see core.h for description).
int findBuiltinFunction(const PKVM* vm, const char* name, uint32_t length) {
   uint32_t i;
   for (i = 0; i < vm->builtins_count; i++) {
     if (length == vm->builtins[i].length &&
       strncmp(name, vm->builtins[i].name, length) == 0) {
       return i;
     }
   }
   return -1;
 }

// getBuiltinFunction implementation (see core.h for description).
Function* getBuiltinFunction(const PKVM* vm, int index) {
  ASSERT_INDEX((uint32_t)index, vm->builtins_count);
  return vm->builtins[index].fn;
}

// getBuiltinFunctionName implementation (see core.h for description).
const char* getBuiltinFunctionName(const PKVM* vm, int index) {
  ASSERT_INDEX((uint32_t)index, vm->builtins_count);
  return vm->builtins[index].name;
}

// getCoreLib implementation (see core.h for description).
Script* getCoreLib(const PKVM* vm, String* name) {
  Var lib = mapGet(vm->core_libs, VAR_OBJ(name));
  if (IS_UNDEF(lib)) return NULL;
  ASSERT(IS_OBJ_TYPE(lib, OBJ_SCRIPT), OOPS);
  return (Script*)AS_OBJ(lib);
}

/*****************************************************************************/
/* CORE BUILTIN FUNCTIONS                                                    */
/*****************************************************************************/

DEF(coreTypeName,
  "type_name(value:var) -> string\n"
  "Returns the type name of the of the value.") {

  RET(VAR_OBJ(newString(vm, varTypeName(ARG(1)))));
}

DEF(coreHelp,
  "help([fn]) -> null\n"
  "This will write an error message to stdout and return null.") {

  int argc = ARGC;
  if (argc != 0 && argc != 1) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (argc == 0) {
    // If there ins't an io function callback, we're done.
    if (vm->config.write_fn == NULL) RET(VAR_NULL);
    vm->config.write_fn(vm, "TODO: print help here\n");

  } else if (argc == 1) {
    Function* fn;
    if (!validateArgFunction(vm, 1, &fn)) return;

    // If there ins't an io function callback, we're done.
    if (vm->config.write_fn == NULL) RET(VAR_NULL);

    if (fn->docstring != NULL) {
      vm->config.write_fn(vm, fn->docstring);
      vm->config.write_fn(vm, "\n\n");
    } else {
      // TODO: A better message.
      vm->config.write_fn(vm, "function '");
      vm->config.write_fn(vm, fn->name);
      vm->config.write_fn(vm, "()' doesn't have a docstring.\n");
    }
  }
}

DEF(coreAssert,
  "assert(condition:bool [, msg:string]) -> void\n"
  "If the condition is false it'll terminate the current fiber with the "
  "optional error message") {

  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (!toBool(ARG(1))) {
    String* msg = NULL;

    if (argc == 2) {
      if (AS_OBJ(ARG(2))->type != OBJ_STRING) {
        msg = toString(vm, ARG(2));
      } else {
        msg = (String*)AS_OBJ(ARG(2));
      }
      vmPushTempRef(vm, &msg->_super);
      VM_SET_ERROR(vm, stringFormat(vm, "Assertion failed: '@'.", msg));
      vmPopTempRef(vm);
    } else {
      VM_SET_ERROR(vm, newString(vm, "Assertion failed."));
    }
  }
}

DEF(coreBin,
  "bin(value:num) -> string\n"
  "Returns as a binary value string with '0x' prefix.") {
  
  char buff[STR_BIN_BUFF_SIZE];
  int64_t value;
  bool negative;
  char* ptr;
  uint32_t length;

  if (!validateInteger(vm, ARG(1), &value, "Argument 1")) return;

  negative = (value < 0) ? true : false;
  if (negative) value = -value;

  ptr = buff + STR_BIN_BUFF_SIZE - 1;
  *ptr-- = '\0'; // NULL byte at the end of the string.

  if (value != 0) {
    while (value > 0) {
      *ptr-- = (char)('0' + (value & 1));
      value >>= 1;
    }
  } else {
    *ptr-- = '0';
  }

  *ptr-- = 'b'; *ptr-- = '0';
  if (negative) *ptr-- = '-';

  length = (uint32_t)((buff + STR_BIN_BUFF_SIZE - 1) - (ptr + 1));
  RET(VAR_OBJ(newStringLength(vm, ptr + 1, length)));
}

DEF(coreHex,
  "hex(value:num) -> string\n"
  "Returns as a hexadecimal value string with '0x' prefix.") {

  char buff[STR_HEX_BUFF_SIZE];
  int64_t value;
  char* ptr;
  uint32_t _x;
  int length;
  
  if (!validateInteger(vm, ARG(1), &value, "Argument 1")) return;

  ptr = buff;
  if (value < 0) *ptr++ = '-';
  *ptr++ = '0'; *ptr++ = 'x';

  if (value > UINT32_MAX || value < -(int64_t)(UINT32_MAX)) {
    VM_SET_ERROR(vm, newString(vm, "Integer is too large."));
    RET(VAR_NULL);
  }

  // TODO: spritnf limits only to 8 character hex value, we need to do it
  // outself for a maximum of 16 character long (see bin() for reference).
  _x = (uint32_t)((value < 0) ? -value : value);
  length = sprintf(ptr, "%x", _x);

  RET(VAR_OBJ(newStringLength(vm, buff,
    (uint32_t)((ptr + length) - (char*)(buff)))));
}

DEF(coreYield,
  "yield([value]) -> var\n"
  "Return the current function with the yield [value] to current running "
  "fiber. If the fiber is resumed, it'll run from the next statement of the "
  "yield() call. If the fiber resumed with with a value, the return value of "
  "the yield() would be that value otherwise null.") {

  int argc = ARGC;
  if (argc > 1) { // yield() or yield(val).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  vmYieldFiber(vm, (argc == 1) ? &ARG(1) : NULL);
}

DEF(coreToString,
  "to_string(value:var) -> string\n"
  "Returns the string representation of the value.") {

  RET(VAR_OBJ(toString(vm, ARG(1))));
}

DEF(corePrint,
  "print(...) -> void\n"
  "Write each argument as space seperated, to the stdout and ends with a "
  "newline.") {
  int i;

  // If the host application doesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

  for (i = 1; i <= ARGC; i++) {
    if (i != 1) vm->config.write_fn(vm, " ");
    vm->config.write_fn(vm, toString(vm, ARG(i))->data);
  }

  vm->config.write_fn(vm, "\n");
}

DEF(coreInput,
  "input([msg:var]) -> string\n"
  "Read a line from stdin and returns it without the line ending. Accepting "
  "an optional argument [msg] and prints it before reading.") {
  PkStringPtr result;
  String* line;

  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  // If the host application doesn't provide any write function, return.
  if (vm->config.read_fn == NULL) return;

  if (argc == 1) {
    vm->config.write_fn(vm, toString(vm, ARG(1))->data);
  }

  result = vm->config.read_fn(vm);
  line = newString(vm, result.string);
  if (result.on_done) result.on_done(vm, &result);
  RET(VAR_OBJ(line));
}

// String functions.
// -----------------

// TODO: substring.

DEF(coreStrChr,
  "str_chr(value:number) -> string\n"
  "Returns the ASCII string value of the integer argument.") {
  int64_t num;
  char c;
  if (!validateInteger(vm, ARG(1), &num, "Argument 1")) return;

  if (!IS_NUM_BYTE(num)) {
    RET_ERR(newString(vm, "The number is not in a byte range."));
  }

  c = (char)num;
  RET(VAR_OBJ(newStringLength(vm, &c, 1)));
}

DEF(coreStrOrd,
  "str_ord(value:string) -> number\n"
  "Returns integer value of the given ASCII character.") {

  String* c;
  if (!validateArgString(vm, 1, &c)) return;
  if (c->length != 1) {
    RET_ERR(newString(vm, "Expected a string of length 1."));

  } else {
    RET(VAR_NUM((double)c->data[0]));
  }
}

// List functions.
// ---------------

DEF(coreListAppend,
  "list_append(self:List, value:var) -> List\n"
  "Append the [value] to the list [self] and return the list.") {

  List* list;
  Var elem;

  if (!validateArgList(vm, 1, &list)) return;
  elem = ARG(2);

  listAppend(vm, list, elem);
  RET(VAR_OBJ(list));
}

// Map functions.
// --------------

DEF(coreMapRemove,
  "map_remove(self:map, key:var) -> var\n"
  "Remove the [key] from the map [self] and return it's value if the key "
  "exists, otherwise it'll return null.") {

  Map* map;
  Var key;

  if (!validateArgMap(vm, 1, &map)) return;
  key = ARG(2);

  RET(mapRemoveKey(vm, map, key));
}

/*****************************************************************************/
/* CORE MODULE METHODS                                                       */
/*****************************************************************************/

// Create a module and add it to the vm's core modules, returns the script.
static Script* newModuleInternal(PKVM* vm, const char* name) {
  Script* scr;

  // Create a new Script for the module.
  String* _name = newString(vm, name);
  vmPushTempRef(vm, &_name->_super);

  // Check if any module with the same name already exists and assert to the
  // hosting application.
  if (!IS_UNDEF(mapGet(vm->core_libs, VAR_OBJ(_name)))) {
    vmPopTempRef(vm); // _name
    __ASSERT(false, stringFormat(vm,
             "A module named '$' already exists", name)->data);
  }

  scr = newScript(vm, _name);
  scr->module = _name;
  vmPopTempRef(vm); // _name

  // Add the script to core_libs.
  vmPushTempRef(vm, &scr->_super);
  mapSet(vm, vm->core_libs, VAR_OBJ(_name), VAR_OBJ(scr));
  vmPopTempRef(vm);

  return scr;
}

// This will fail an assertion if a function or a global with the [name]
// already exists in the module.
static inline void assertModuleNameDef(PKVM* vm, Script* script,
                                       const char* name) {
  // Check if function with the same name already exists.
  if (scriptGetFunc(script, name, (uint32_t)strlen(name)) != -1) {
    __ASSERT(false, stringFormat(vm, "A function named '$' already esists "
      "on module '@'", name, script->module)->data);
  }

  // Check if a global variable with the same name already exists.
  if (scriptGetGlobals(script, name, (uint32_t)strlen(name)) != -1) {
    __ASSERT(false, stringFormat(vm, "A global variable named '$' already "
      "esists on module '@'", name, script->module)->data);
  }
}

// The internal function to add global value to a module.
static void moduleAddGlobalInternal(PKVM* vm, Script* script,
                                    const char* name, Var value) {

  // Ensure the name isn't defined already.
  assertModuleNameDef(vm, script, name);

  // Add the value to the globals buffer.
  scriptAddGlobal(vm, script, name, (uint32_t)strlen(name), value);
}

// An internal function to add a function to the given [script].
static void moduleAddFunctionInternal(PKVM* vm, Script* script,
                                      const char* name, pkNativeFn fptr,
                                      int arity, const char* docstring) {

  Function* fn;

  // Ensure the name isn't predefined.
  assertModuleNameDef(vm, script, name);

  fn = newFunction(vm, name, (int)strlen(name),
                             script, true, docstring);
  fn->native = fptr;
  fn->arity = arity;
}

// TODO: make the below module functions as PK_DOC(name, doc);

// 'lang' library methods.
// -----------------------

DEF(stdLangClock,
  "clock() -> num\n"
  "Returns the number of seconds since the application started") {

  RET(VAR_NUM((double)clock() / CLOCKS_PER_SEC));
}

DEF(stdLangGC,
  "gc() -> num\n"
  "Trigger garbage collection and return the amount of bytes cleaned.") {

  size_t garbage;
  size_t bytes_before = vm->bytes_allocated;
  vmCollectGarbage(vm);
  garbage = bytes_before - vm->bytes_allocated;
  RET(VAR_NUM((double)garbage));
}

DEF(stdLangDisas,
  "disas(fn:Function) -> String\n"
  "Returns the disassembled opcode of the function [fn].") {

  String* dump;
  pkByteBuffer buff;
  Function* fn;

  if (!validateArgFunction(vm, 1, &fn)) return;

  pkByteBufferInit(&buff);
  dumpFunctionCode(vm, fn, &buff);
  dump = newString(vm, (const char*)buff.data);
  pkByteBufferClear(&buff, vm);

  RET(VAR_OBJ(dump));
}

#ifdef DEBUG
DEF(stdLangDebugBreak,
  "debug_break() -> null\n"
  "A debug function for development (will be removed).") {

  DEBUG_BREAK();
}
#endif

DEF(stdLangWrite,
  "write(...) -> null\n"
  "Write function, just like print function but it wont put space between"
  "args and write a new line at the end.") {

  int i;
  String* str; //< Will be cleaned by garbage collector;

  // If the host application doesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

  for (i = 1; i <= ARGC; i++) {
    Var arg = ARG(i);
    // If it's already a string don't allocate a new string instead use it.
    if (IS_OBJ_TYPE(arg, OBJ_STRING)) {
      str = (String*)AS_OBJ(arg);
    } else {
      str = toString(vm, arg);
    }

    vm->config.write_fn(vm, str->data);
  }
}

// 'math' library methods.
// -----------------------

DEF(stdMathFloor,
  "floor(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(floor(num)));
}

DEF(stdMathCeil,
  "ceil(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(ceil(num)));
}

DEF(stdMathPow,
  "pow(value:num) -> num\n") {

  double num, ex;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (!validateNumeric(vm, ARG(2), &ex, "Argument 2")) return;
  RET(VAR_NUM(pow(num, ex)));
}

DEF(stdMathSqrt,
  "sqrt(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(sqrt(num)));
}

DEF(stdMathAbs,
  "abs(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (num < 0) num = -num;
  RET(VAR_NUM(num));
}

DEF(stdMathSign,
  "sign(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (num < 0) num = -1;
  else if (num > 0) num = +1;
  else num = 0;
  RET(VAR_NUM(num));
}

DEF(stdMathHash,
  "hash(value:var) -> num\n"
  "Return the hash value of the variable, if it's not hashable it'll "
  "return null.") {

  if (IS_OBJ(ARG(1))) {
    if (!isObjectHashable(AS_OBJ(ARG(1))->type)) {
      RET(VAR_NULL);
    }
  }
  RET(VAR_NUM((double)varHashValue(ARG(1))));
}

DEF(stdMathSine,
  "sin(rad:num) -> num\n"
  "Return the sine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(sin(rad)));
}

DEF(stdMathCosine,
  "cos(rad:num) -> num\n"
  "Return the cosine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(cos(rad)));
}

DEF(stdMathTangent,
  "tan(rad:num) -> num\n"
  "Return the tangent value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(tan(rad)));
}

// 'Fiber' module methods.
// -----------------------

DEF(stdFiberNew,
  "new(fn:Function) -> fiber\n"
  "Create and return a new fiber from the given function [fn].") {

  Function* fn;
  if (!validateArgFunction(vm, 1, &fn)) return;
  RET(VAR_OBJ(newFiber(vm, fn)));
}

DEF(stdFiberRun,
  "run(fb:Fiber, ...) -> var\n"
  "Runs the fiber's function with the provided arguments and returns it's "
  "return value or the yielded value if it's yielded.") {

  // Buffer of argument to call vmPrepareFiber().
  Var* args[MAX_ARGC];
  Fiber* fb;
  int i;

  int argc = ARGC;
  if (argc == 0) // Missing the fiber argument.
    RET_ERR(newString(vm, "Missing argument - fiber."));

  if (!validateArgFiber(vm, 1, &fb)) return;

  // ARG(1) is fiber, function arguments are ARG(2), ARG(3), ... ARG(argc).
  for (i = 1; i < argc; i++) {
    args[i - 1] = &ARG(i + 1);
  }

  // Switch fiber and start execution.
  if (vmPrepareFiber(vm, fb, argc - 1, args)) {
    ASSERT(fb == vm->fiber, OOPS);
    fb->state = FIBER_RUNNING;
  }
}

DEF(stdFiberResume,
  "fiber_resume(fb:Fiber) -> var\n"
  "Resumes a yielded function from a previous call of fiber_run() function. "
  "Return it's return value or the yielded value if it's yielded.") {

  Var value;
  Fiber* fb;
  int argc = ARGC;
  if (argc == 0) // Missing the fiber argument.
    RET_ERR(newString(vm, "Expected at least 1 argument(s)."));
  if (argc > 2) // Can only accept 1 argument for resume.
    RET_ERR(newString(vm, "Expected at most 2 argument(s)."));

  if (!validateArgFiber(vm, 1, &fb)) return;

  value = (argc == 1) ? VAR_NULL : ARG(2);

  // Switch fiber and resume execution.
  if (vmSwitchFiber(vm, fb, &value)) {
    ASSERT(fb == vm->fiber, OOPS);
    fb->state = FIBER_RUNNING;
  }
}

/*****************************************************************************/
/* CORE INITIALIZATION                                                       */
/*****************************************************************************/

static void initializeBuiltinFN(PKVM* vm, BuiltinFn* bfn, const char* name,
                                int length, int arity, pkNativeFn ptr,
                                const char* docstring) {
  bfn->name = name;
  bfn->length = length;

  bfn->fn = newFunction(vm, name, length, NULL, true, docstring);
  bfn->fn->arity = arity;
  bfn->fn->native = ptr;
}

void initializeCore(PKVM* vm) {

#define INITIALIZE_BUILTIN_FN(name, fn, argc)                        \
  initializeBuiltinFN(vm, &vm->builtins[vm->builtins_count++], name, \
                      (int)strlen(name), argc, fn, DOCSTRING(fn));

#define MODULE_ADD_FN(module, name, fn, argc) \
  moduleAddFunctionInternal(vm, module, name, fn, argc, DOCSTRING(fn))

  // Initialize builtin functions.
  INITIALIZE_BUILTIN_FN("type_name",   coreTypeName,   1);

  // TODO: Add is keyword with modules for builtin types.
  // ex: val is Num; val is null; val is List; val is Range
  //     List.append(l, e) # List is implicitly imported core module.
  //     String.lower(s)

  INITIALIZE_BUILTIN_FN("help",        coreHelp,      -1);
  INITIALIZE_BUILTIN_FN("assert",      coreAssert,    -1);
  INITIALIZE_BUILTIN_FN("bin",         coreBin,        1);
  INITIALIZE_BUILTIN_FN("hex",         coreHex,        1);
  INITIALIZE_BUILTIN_FN("yield",       coreYield,     -1);
  INITIALIZE_BUILTIN_FN("to_string",   coreToString,   1);
  INITIALIZE_BUILTIN_FN("print",       corePrint,     -1);
  INITIALIZE_BUILTIN_FN("input",       coreInput,     -1);

  // String functions.
  INITIALIZE_BUILTIN_FN("str_chr",     coreStrChr,     1);
  INITIALIZE_BUILTIN_FN("str_ord",     coreStrOrd,     1);

  // List functions.
  INITIALIZE_BUILTIN_FN("list_append", coreListAppend, 2);

  // Map functions.
  INITIALIZE_BUILTIN_FN("map_remove",  coreMapRemove,  2);

  // Core Modules /////////////////////////////////////////////////////////////
  {
  Script* lang = newModuleInternal(vm, "lang");
  MODULE_ADD_FN(lang, "clock", stdLangClock,  0);
  MODULE_ADD_FN(lang, "gc",    stdLangGC,     0);
  MODULE_ADD_FN(lang, "disas", stdLangDisas,  1);
  MODULE_ADD_FN(lang, "write", stdLangWrite, -1);
#ifdef DEBUG
  MODULE_ADD_FN(lang, "debug_break", stdLangDebugBreak, 0);
#endif
  }

  {
  Script* math = newModuleInternal(vm, "math");
  MODULE_ADD_FN(math, "floor", stdMathFloor,   1);
  MODULE_ADD_FN(math, "ceil",  stdMathCeil,    1);
  MODULE_ADD_FN(math, "pow",   stdMathPow,     2);
  MODULE_ADD_FN(math, "sqrt",  stdMathSqrt,    1);
  MODULE_ADD_FN(math, "abs",   stdMathAbs,     1);
  MODULE_ADD_FN(math, "sign",  stdMathSign,    1);
  MODULE_ADD_FN(math, "hash",  stdMathHash,    1);
  MODULE_ADD_FN(math, "sin",   stdMathSine,    1);
  MODULE_ADD_FN(math, "cos",   stdMathCosine,  1);
  MODULE_ADD_FN(math, "tan",   stdMathTangent, 1);
  // TODO: low priority - sinh, cosh, tanh, asin, acos, atan.
  
  // Note that currently it's mutable (since it's a global variable, not
  // constant and pocketlang doesn't support constant) so the user shouldn't
  // modify the PI, like in python.
  // TODO: at varSetAttrib() we can detect if the user try to change an
  // attribute of a core module and we can throw an error.
  moduleAddGlobalInternal(vm, math, "PI", VAR_NUM(M_PI));
  }

  {
  Script* fiber = newModuleInternal(vm, "Fiber");
  MODULE_ADD_FN(fiber, "new",      stdFiberNew,     1);
  MODULE_ADD_FN(fiber, "run",      stdFiberRun,    -1);
  MODULE_ADD_FN(fiber, "resume",   stdFiberResume, -1);
  }
}

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

#define UNSUPPORTED_OPERAND_TYPES(op)                                  \
  VM_SET_ERROR(vm, stringFormat(vm, "Unsupported operand types for "   \
    "operator '" op "' $ and $", varTypeName(v1), varTypeName(v2)))

#define RIGHT_OPERAND "Right operand"

Var varAdd(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(d1 + d2);
    }
    return VAR_NULL;
  }

  if (IS_OBJ(v1) && IS_OBJ(v2)) {
    Object *o1 = AS_OBJ(v1), *o2 = AS_OBJ(v2);
    switch (o1->type) {

      case OBJ_STRING:
      {
        if (o2->type == OBJ_STRING) {
          return VAR_OBJ(stringJoin(vm, (String*)o1, (String*)o2));
        }
      } break;

      case OBJ_LIST:
      {
        if (o2->type == OBJ_LIST) {
          return VAR_OBJ(listJoin(vm, (List*)o1, (List*)o2));
        }
      } break;

      case OBJ_MAP:
      case OBJ_RANGE:
      case OBJ_SCRIPT:
      case OBJ_FUNC:
      case OBJ_FIBER:
      case OBJ_CLASS:
      case OBJ_INST:
        break;
    }
  }

  UNSUPPORTED_OPERAND_TYPES("+");
  return VAR_NULL;
}

Var varSubtract(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(d1 - d2);
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("-");
  return VAR_NULL;
}

Var varMultiply(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(d1 * d2);
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("*");
  return VAR_NULL;
}

Var varDivide(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(d1 / d2);
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("/");
  return VAR_NULL;
}

Var varModulo(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(fmod(d1, d2));
    }
    return VAR_NULL;
  }

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    //const String* str = (const String*)AS_OBJ(v1);
    TODO; // "fmt" % v2.
  }

  UNSUPPORTED_OPERAND_TYPES("%");
  return VAR_NULL;
}

Var varBitAnd(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 & i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("&");
  return VAR_NULL;
}

Var varBitOr(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 | i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("|");
  return VAR_NULL;
}

Var varBitXor(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 ^ i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("^");
  return VAR_NULL;
}

Var varBitLshift(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 << i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("<<");
  return VAR_NULL;
}

Var varBitRshift(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 >> i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES(">>");
  return VAR_NULL;
}

Var varBitNot(PKVM* vm, Var v) {
  int64_t i;
  if (!validateInteger(vm, v, &i, "Unary operand")) return VAR_NULL;
  return VAR_NUM((double)(~i));
}

bool varGreater(Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1) && isNumeric(v2, &d2)) {
    return d1 > d2;
  }

  TODO;
  return false;
}

bool varLesser(Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1) && isNumeric(v2, &d2)) {
    return d1 < d2;
  }

  TODO;
  return false;
}

#undef RIGHT_OPERAND
#undef UNSUPPORTED_OPERAND_TYPES

bool varContains(PKVM* vm, Var elem, Var container) {
  Object* obj;
  if (!IS_OBJ(container)) {
    VM_SET_ERROR(vm, stringFormat(vm, "'$' is not iterable.",
                 varTypeName(container)));
  }
  obj = AS_OBJ(container);

  switch (obj->type) {
    case OBJ_STRING: {
	  String* sub;
	  String* str;

      if (!IS_OBJ_TYPE(elem, OBJ_STRING)) {
        VM_SET_ERROR(vm, stringFormat(vm, "Expected a string operand."));
        return false;
      }

      sub = (String*)AS_OBJ(elem);
      str = (String*)AS_OBJ(container);
      if (sub->length > str->length) return false;

      TODO;

    } break;

    case OBJ_LIST: {
	  uint32_t i;
      List* list = (List*)AS_OBJ(container);
      for (i = 0; i < list->elements.count; i++) {
        if (isValuesEqual(elem, list->elements.data[i])) return true;
      }
      return false;
    } break;

    case OBJ_MAP: {
      Map* map = (Map*)AS_OBJ(container);
      return !IS_UNDEF(mapGet(map, elem));
    } break;

    case OBJ_RANGE:
    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_CLASS:
    case OBJ_INST:
      TODO;
  }
  UNREACHABLE();
}

// Here we're switching the FNV-1a hash value of the name (cstring). Which is
// an efficient way than having multiple if (attrib == "name"). From O(n) * k
// to O(1) where n is the length of the string and k is the number of string
// comparison.
//
// ex:
//     SWITCH_ATTRIB(str) { // str = "length"
//       CASE_ATTRIB("length", 0x83d03615) : { return string->length; }
//     }
//
// In C++11 this can be achieved (in a better way) with user defined literals
// and constexpr. (Reference from my previous compiler written in C++).
// https://github.com/ThakeeNathees/carbon/
//
// However there is a python script that's matching the CASE_ATTRIB() macro
// calls and validate if the string and the hash values are matching.
// TODO: port it to the CI/CD process at github actions.
//
#define SWITCH_ATTRIB(name) switch (utilHashString(name))
#define CASE_ATTRIB(name, hash) case hash
#define CASE_DEFAULT default

// Set error for accessing non-existed attribute.
#define ERR_NO_ATTRIB(vm, on, attrib)                                        \
  VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no attribute named '$'", \
                                varTypeName(on), attrib->data))

Var varGetAttrib(PKVM* vm, Var on, String* attrib) {

  Object* obj;
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return VAR_NULL;
  }

  obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
    {
      String* str = (String*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("length", 0x83d03615) :
          return VAR_NUM((double)(str->length));

        CASE_ATTRIB("lower", 0xb51d04ba) :
          return VAR_OBJ(stringLower(vm, str));

        CASE_ATTRIB("upper", 0xa8c6a47) :
          return VAR_OBJ(stringUpper(vm, str));

        CASE_ATTRIB("strip", 0xfd1b18d1) :
          return VAR_OBJ(stringStrip(vm, str));

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }

      UNREACHABLE();
    }

    case OBJ_LIST:
    {
      List* list = (List*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("length", 0x83d03615) :
          return VAR_NUM((double)(list->elements.count));

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }

      UNREACHABLE();
    }

    case OBJ_MAP:
    {
      // Not sure should I allow string values could be accessed with
      // this way. ex:
      // map = { "foo" : 42, "can't access" : 32 }
      // val = map.foo ## 42
      TODO;
      UNREACHABLE();
    }

    case OBJ_RANGE:
    {
      Range* range = (Range*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("as_list", 0x1562c22):
          return VAR_OBJ(rangeAsList(vm, range));

        // We can't use 'start', 'end' since 'end' in pocketlang is a
        // keyword. Also we can't use 'from', 'to' since 'from' is a keyword
        // too. So, we're using 'first' and 'last' to access the range limits.

        CASE_ATTRIB("first", 0x4881d841):
          return VAR_NUM(range->from);

        CASE_ATTRIB("last", 0x63e1d819):
          return VAR_NUM(range->to);

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }

      UNREACHABLE();
    }

    case OBJ_SCRIPT:
    {
      Script* scr = (Script*)obj;

      // Search in types.
      int index = scriptGetClass(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->classes.count);
        return VAR_OBJ(scr->classes.data[index]);
      }

      // Search in functions.
      index = scriptGetFunc(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->functions.count);
        return VAR_OBJ(scr->functions.data[index]);
      }

      // Search in globals.
      index = scriptGetGlobals(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->globals.count);
        return scr->globals.data[index];
      }

      ERR_NO_ATTRIB(vm, on, attrib);
      return VAR_NULL;
    }

    case OBJ_FUNC:
    {
      Function* fn = (Function*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("arity", 0x3e96bd7a) :
          return VAR_NUM((double)(fn->arity));

        CASE_ATTRIB("name", 0x8d39bde6) :
          return VAR_OBJ(newString(vm, fn->name));

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }
      UNREACHABLE();
    }

    case OBJ_FIBER:
      {
        Fiber* fb = (Fiber*)obj;
        SWITCH_ATTRIB(attrib->data) {

          CASE_ATTRIB("is_done", 0x789c2706):
            return VAR_BOOL(fb->state == FIBER_DONE);

          CASE_ATTRIB("function", 0x9ed64249):
            return VAR_OBJ(fb->func);

          CASE_DEFAULT:
            ERR_NO_ATTRIB(vm, on, attrib);
            return VAR_NULL;
          }
        UNREACHABLE();
      }

    case OBJ_CLASS:
      TODO;
      UNREACHABLE();

    case OBJ_INST:
    {
      Instance* inst = (Instance*)obj;
      if (inst->is_native) {
        TODO;

      } else {
        uint32_t i;

        // TODO: Optimize this with binary search.
        Class* ty = inst->ins->type;
        for (i = 0; i < ty->field_names.count; i++) {
          String* f_name;
		  ASSERT_INDEX(i, ty->field_names.count);
          ASSERT_INDEX(ty->field_names.data[i], ty->owner->names.count);
          f_name = ty->owner->names.data[ty->field_names.data[i]];
          if (f_name->hash == attrib->hash &&
              f_name->length == attrib->length &&
              memcmp(f_name->data, attrib->data, attrib->length) == 0) {
            return inst->ins->fields.data[i];
          }
        }
      }

      ERR_NO_ATTRIB(vm, on, attrib);
      return VAR_NULL;
    }

    default:
      UNREACHABLE();
  }

  UNREACHABLE();
}

void varSetAttrib(PKVM* vm, Var on, String* attrib, Var value) {

#define ATTRIB_IMMUTABLE(name)                                                \
do {                                                                          \
  if ((attrib->length == strlen(name) && strcmp(name, attrib->data) == 0)) {  \
    VM_SET_ERROR(vm, stringFormat(vm, "'$' attribute is immutable.", name));  \
    return;                                                                   \
  }                                                                           \
} while (false)

  Object* obj;
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return;
  }

  obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      ATTRIB_IMMUTABLE("length");
      ATTRIB_IMMUTABLE("lower");
      ATTRIB_IMMUTABLE("upper");
      ATTRIB_IMMUTABLE("strip");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_LIST:
      ATTRIB_IMMUTABLE("length");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_MAP:
      // Not sure should I allow string values could be accessed with
      // this way. ex:
      // map = { "foo" : 42, "can't access" : 32 }
      // map.foo = 'bar'
      TODO;

      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_RANGE:
      ATTRIB_IMMUTABLE("as_list");
      ATTRIB_IMMUTABLE("first");
      ATTRIB_IMMUTABLE("last");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_SCRIPT: {
      Script* scr = (Script*)obj;

      // Check globals.
      int index = scriptGetGlobals(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->globals.count);
        scr->globals.data[index] = value;
        return;
      }

      // Check function (Functions are immutable).
      index = scriptGetFunc(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->functions.count);
        ATTRIB_IMMUTABLE(scr->functions.data[index]->name);
        return;
      }

      index = scriptGetClass(scr, attrib->data, attrib->length);
      if (index != -1) {
		String* name;
        ASSERT_INDEX((uint32_t)index, scr->classes.count);
        ASSERT_INDEX(scr->classes.data[index]->name, scr->names.count);
        name = scr->names.data[scr->classes.data[index]->name];
        ATTRIB_IMMUTABLE(name->data);
        return;
      }

      ERR_NO_ATTRIB(vm, on, attrib);
      return;
    }

    case OBJ_FUNC:
      ATTRIB_IMMUTABLE("arity");
      ATTRIB_IMMUTABLE("name");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_FIBER:
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_CLASS:
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_INST:
    {
      Instance* inst = (Instance*)obj;
      if (inst->is_native) {
        TODO;
        return;

      } else {
        uint32_t i;

        // TODO: Optimize this with binary search.
        Class* ty = inst->ins->type;
        for (i = 0; i < ty->field_names.count; i++) {
		  String* f_name;
          ASSERT_INDEX(i, ty->field_names.count);
          ASSERT_INDEX(ty->field_names.data[i], ty->owner->names.count);
          f_name = ty->owner->names.data[ty->field_names.data[i]];
          if (f_name->hash == attrib->hash &&
            f_name->length == attrib->length &&
            memcmp(f_name->data, attrib->data, attrib->length) == 0) {
            inst->ins->fields.data[i] = value;
            return;
          }
        }
        ERR_NO_ATTRIB(vm, on, attrib);
        return;
      }

      UNREACHABLE();
    }

    default:
      UNREACHABLE();
  }
  UNREACHABLE();

#undef ATTRIB_IMMUTABLE
}

#undef SWITCH_ATTRIB
#undef CASE_ATTRIB
#undef CASE_DEFAULT
#undef ERR_NO_ATTRIB

Var varGetSubscript(PKVM* vm, Var on, Var key) {
  Object* obj;
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return VAR_NULL;
  }

  obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
    {
	  String* c;
      int64_t index;
      String* str = ((String*)obj);
      if (!validateInteger(vm, key, &index, "List index")) {
        return VAR_NULL;
      }
      if (!validateIndex(vm, index, str->length, "String")) {
        return VAR_NULL;
      }
      c = newStringLength(vm, str->data + index, 1);
      return VAR_OBJ(c);
    }

    case OBJ_LIST:
    {
      int64_t index;
      pkVarBuffer* elems = &((List*)obj)->elements;
      if (!validateInteger(vm, key, &index, "List index")) {
        return VAR_NULL;
      }
      if (!validateIndex(vm, index, elems->count, "List")) {
        return VAR_NULL;
      }
      return elems->data[index];
    }

    case OBJ_MAP:
    {
      Var value = mapGet((Map*)obj, key);
      if (IS_UNDEF(value)) {

        String* key_str = toString(vm, key);
        vmPushTempRef(vm, &key_str->_super);
        if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
          VM_SET_ERROR(vm, stringFormat(vm, "Invalid key '@'.", key_str));
        } else {
          VM_SET_ERROR(vm, stringFormat(vm, "Key '@' not exists", key_str));
        }
        vmPopTempRef(vm);
        return VAR_NULL;
      }
      return value;
    }

    case OBJ_RANGE:
    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_CLASS:
    case OBJ_INST:
      TODO;
      UNREACHABLE();

    default:
      UNREACHABLE();
  }

  UNREACHABLE();
}

void varsetSubscript(PKVM* vm, Var on, Var key, Var value) {
  Object* obj;
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return;
  }

  obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      VM_SET_ERROR(vm, newString(vm, "String objects are immutable."));
      return;

    case OBJ_LIST:
    {
      int64_t index;
      pkVarBuffer* elems = &((List*)obj)->elements;
      if (!validateInteger(vm, key, &index, "List index")) return;
      if (!validateIndex(vm, index, elems->count, "List")) return;
      elems->data[index] = value;
      return;
    }

    case OBJ_MAP:
    {
      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        VM_SET_ERROR(vm, stringFormat(vm, "$ type is not hashable.",
                                      varTypeName(key)));
      } else {
        mapSet(vm, (Map*)obj, key, value);
      }
      return;
    }

    case OBJ_RANGE:
    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_CLASS:
    case OBJ_INST:
      TODO;
      UNREACHABLE();

    default:
      UNREACHABLE();
  }

  UNREACHABLE();
}

#undef DOCSTRING
#undef DEF
