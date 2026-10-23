#pragma once

#define EXPORT __attribute__((visibility("default")))

void FCSystemLoader_onLibConstructor(const char* soname);
void FCSystemLoader_onLibDestructor(const char* soname);

void* FCSystemLoader_loadSystem(
    const char* soname,
    int symbolCount,
    const char* functionNames[symbolCount],
    void (*functionPointers[symbolCount])(void)
);
void FCSystemLoader_unloadSystem(void* lib);

void* FCSystemLoader_getFunction(void* lib, const char* soname, const char* name);
void* __attribute__((noreturn)) FCSystemLoader_unavailable(const char* name);
