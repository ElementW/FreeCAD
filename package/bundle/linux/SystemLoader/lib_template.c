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

typedef struct xcb_extension_t {
    const char *name;
    int global_id;
} xcb_extension_t;

#ifdef SONAME_libxcb_so_1
EXPORT xcb_extension_t xcb_big_requests_id = { "BIG-REQUESTS", 0 };
EXPORT xcb_extension_t xcb_xc_misc_id = { "XC-MISC", 0 };
#endif
#ifdef SONAME_libxcb_composite_so_0
EXPORT xcb_extension_t xcb_composite_id = { "Composite", 0 };
#endif
#ifdef SONAME_libxcb_damage_so_0
EXPORT xcb_extension_t xcb_damage_id = { "DAMAGE", 0 };
#endif
#ifdef SONAME_libxcb_dbe_so_0
EXPORT xcb_extension_t xcb_dbe_id = { "DOUBLE-BUFFER", 0 };
#endif
#ifdef SONAME_libxcb_dpms_so_0
EXPORT xcb_extension_t xcb_dpms_id = { "DPMS", 0 };
#endif
#ifdef SONAME_libxcb_dri2_so_0
EXPORT xcb_extension_t xcb_dri2_id = { "DRI2", 0 };
#endif
#ifdef SONAME_libxcb_dri3_so_0
EXPORT xcb_extension_t xcb_dri3_id = { "DRI3", 0 };
#endif
#ifdef SONAME_libxcb_glx_so_0
EXPORT xcb_extension_t xcb_glx_id = { "GLX", 0 };
#endif
#ifdef SONAME_libxcb_xinput_so_0
EXPORT xcb_extension_t xcb_input_id = { "XInputExtension", 0 };
#endif
#ifdef SONAME_libxcb_present_so_0
EXPORT xcb_extension_t xcb_present_id = { "Present", 0 };
#endif
#ifdef SONAME_libxcb_randr_so_0
EXPORT xcb_extension_t xcb_randr_id = { "RANDR", 0 };
#endif
#ifdef SONAME_libxcb_record_so_0
EXPORT xcb_extension_t xcb_record_id = { "RECORD", 0 };
#endif
#ifdef SONAME_libxcb_render_so_0
EXPORT xcb_extension_t xcb_render_id = { "RENDER", 0 };
#endif
#ifdef SONAME_libxcb_res_so_0
EXPORT xcb_extension_t xcb_res_id = { "X-R_so_rce", 0 };
#endif
#ifdef SONAME_libxcb_screensaver_so_0
EXPORT xcb_extension_t xcb_screensaver_id = { "MIT-SCREEN-SAVER", 0 };
#endif
#ifdef SONAME_libxcb_shape_so_0
EXPORT xcb_extension_t xcb_shape_id = { "SHAPE", 0 };
#endif
#ifdef SONAME_libxcb_shm_so_0
EXPORT xcb_extension_t xcb_shm_id = { "MIT-SHM", 0 };
#endif
#ifdef SONAME_libxcb_sync_so_1
EXPORT xcb_extension_t xcb_sync_id = { "SYNC", 0 };
#endif
#ifdef SONAME_libxcb_xtest_so_0
EXPORT xcb_extension_t xcb_test_id = { "XTEST", 0 };
#endif
#ifdef SONAME_libxcb_xf86dri_so_0
EXPORT xcb_extension_t xcb_xf86dri_id = { "XFree86-DRI", 0 };
#endif
#ifdef SONAME_libxcb_xfixes_so_0
EXPORT xcb_extension_t xcb_xfixes_id = { "XFIXES", 0 };
#endif
#ifdef SONAME_libxcb_xinerama_so_0
EXPORT xcb_extension_t xcb_xinerama_id = { "XINERAMA", 0 };
#endif
#ifdef SONAME_libxcb_xkb_so_1
EXPORT xcb_extension_t xcb_xkb_id = { "XKEYBOARD", 0 };
#endif
#ifdef SONAME_libxcb_xv_so_0
EXPORT xcb_extension_t xcb_xv_id = { "XVideo", 0 };
#endif
#ifdef SONAME_libxcb_xvmc_so_0
EXPORT xcb_extension_t xcb_xvmc_id = { "XVideo-MotionCompensation", 0 };
#endif
