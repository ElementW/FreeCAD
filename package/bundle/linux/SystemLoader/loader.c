#define _GNU_SOURCE

#include <dlfcn.h>
#include <inttypes.h>
#include <link.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "loader.h"

static uint64_t millis(void)
{
    struct timeval tv = {0, 0};
    gettimeofday(&tv, NULL);
    return (((uint64_t)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}
static const char* debugPrefix = "FreeCAD AppImage system loader";
static bool isDebug(void)
{
    return getenv("FREECAD_APPIMAGE_SYSTEM_LOADER_DEBUG") != NULL;
}

static Lmid_t systemNamespace = LM_ID_NEWLM;
static pthread_mutex_t dlopenMutex = PTHREAD_MUTEX_INITIALIZER;

EXPORT void FCSystemLoader_onLibConstructor(const char* soname)
{
    if (isDebug()) {
        fprintf(stderr, "%s: AppImage-side %s is loading\n", debugPrefix, soname);
    }
}

EXPORT void FCSystemLoader_onLibDestructor(const char* soname)
{
    if (isDebug()) {
        fprintf(stderr, "%s: AppImage-side %s is unloading\n", debugPrefix, soname);
    }
}

void __attribute__((constructor)) FCSystemLoader_init(void)
{
    // Check if we've been appended a DT_RPATH or DT_RUNPATH.
    // If we do, `dlmopen(3)` will potentially attempt loading from the
    // AppImage first, causing a loop.
    Dl_info selfInfo;
    struct link_map* selfLinkMap;
    if (dladdr1((const void*)&FCSystemLoader_init, &selfInfo, (void**)&selfLinkMap, RTLD_DL_LINKMAP)
        == 0) {
        ElfW(Dyn)* dynEntry = selfLinkMap->l_ld;
        while (dynEntry->d_un.d_val != DT_NULL) {
            if (dynEntry->d_tag == DT_RPATH || dynEntry->d_tag == DT_RUNPATH) {
                fprintf(
                    stderr,
                    "%s was built with a DT_RPATH or DT_RUNPATH; it must be unset to load "
                    "system-side libraries\n",
                    debugPrefix
                );
                exit(1);
            }
            dynEntry += 1;
        }
    }

    pthread_mutex_init(&dlopenMutex, NULL);
}

void __attribute__((destructor)) FCSystemLoader_fini(void)
{
    pthread_mutex_destroy(&dlopenMutex);
}

EXPORT void* FCSystemLoader_loadSystem(
    const char* soname,
    int symbolCount,
    const char* functionNames[symbolCount],
    void (*functionPointers[symbolCount])(void)
)
{
    pthread_mutex_lock(&dlopenMutex);
    void* systemLib = NULL;
    if (systemNamespace == LM_ID_NEWLM) {
        systemLib = dlmopen(LM_ID_NEWLM, soname, RTLD_LAZY);
        if (systemLib == NULL) {
            fprintf(
                stderr,
                "%s: failed to load system-side %s and init namespace: %s\n",
                debugPrefix,
                soname,
                dlerror()
            );
            exit(1);
        }
        if (dlinfo(systemLib, RTLD_DI_LMID, &systemNamespace) < 0) {
            fprintf(
                stderr,
                "%s: failed get system-side namespace for %s: %s\n",
                debugPrefix,
                soname,
                dlerror()
            );
            exit(1);
        }
    }
    else {
        // Use RTLD_NOLOAD to not increase the glibc-side dlopen refcount beyond 1.
        systemLib = dlmopen(systemNamespace, soname, RTLD_LAZY | RTLD_NOLOAD);
        if (systemLib == NULL) {
            systemLib = dlmopen(systemNamespace, soname, RTLD_LAZY);
        }
        if (systemLib == NULL) {
            fprintf(stderr, "%s: failed to load system-side %s: %s\n", debugPrefix, soname, dlerror());
            exit(1);
        }
    }
    uint64_t start = 0;
    if (isDebug()) {
        fprintf(stderr, "%s: loaded system-side %s in namespace %ld\n", debugPrefix, soname, systemNamespace);
        start = millis();
    }
    for (int i = 0; i < symbolCount; ++i) {
        functionPointers[i] = (void (*)(void)
        )FCSystemLoader_getFunction(systemLib, soname, functionNames[i]);
    }
    if (isDebug()) {
        uint64_t end = millis();
        fprintf(
            stderr,
            "%s: loaded %d %s symbols in %" PRIu64 " ms\n",
            debugPrefix,
            symbolCount,
            soname,
            end - start
        );
    }
    pthread_mutex_unlock(&dlopenMutex);
    return systemLib;
}

EXPORT void FCSystemLoader_unloadSystem(void* lib)
{
    pthread_mutex_lock(&dlopenMutex);
    dlclose(lib);
    pthread_mutex_unlock(&dlopenMutex);
}

EXPORT void* FCSystemLoader_getFunction(void* lib, const char* soname, const char* name)
{
    void* fn = dlsym(lib, name);
    if (fn == NULL) {
        fprintf(stderr, "%s: could not find function %s in system-side %s\n", debugPrefix, name, soname);
        exit(1);
    }
    return fn;
}

EXPORT void* __attribute__((noreturn)) FCSystemLoader_unavailable(const char* name)
{
    fprintf(stderr, "%s: function %s removed from distribution was called\n", debugPrefix, name);
    exit(1);
}
