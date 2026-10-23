#include <stdlib.h>

#include "loader.h"

#if defined(KIND_FORWARDER)

static void* systemLib = NULL;
static void (*systemFunctions[SYMBOL_COUNT])(void) = {NULL};

# define FN_NAME_AS_STR(fnName, fnIndex) #fnName,
static const char* functionNames[SYMBOL_COUNT] = {SYMBOL_LIST(FN_NAME_AS_STR)};

static void __attribute__((constructor)) _init(void)
{
    FCSystemLoader_onLibConstructor(SONAME);
    systemLib = FCSystemLoader_loadSystem(SONAME, SYMBOL_COUNT, functionNames, systemFunctions);
}

static void __attribute__((destructor)) _fini(void)
{
    if (systemLib != NULL) {
        FCSystemLoader_unloadSystem(systemLib);
        systemLib = NULL;
    }
    FCSystemLoader_onLibDestructor(SONAME);
}

# define MAKE_FORWARDER(fnName, fnIndex) \
     EXPORT void fnName(void) \
     { \
         systemFunctions[fnIndex](); \
     }
SYMBOL_LIST(MAKE_FORWARDER)

#elif defined(KIND_CRASHER)

# define MAKE_CRASHER(fnName, fnIndex) \
     EXPORT void __attribute__((noreturn)) fnName(void) \
     { \
         FCSystemLoader_unavailable(#fnName); \
     }
SYMBOL_LIST(MAKE_CRASHER)

#else
# error Unknown library kind
#endif
