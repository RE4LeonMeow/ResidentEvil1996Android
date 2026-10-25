// MarniGLFuncs.cpp - runtime loader for the GL entry points MarniDX_GL uses.
#include "MarniGLFuncs.h"

#include <stdio.h>

#define RE1_GL_DEF(type, name) type name = nullptr;
RE1_GL_FUNCS(RE1_GL_DEF)
#undef RE1_GL_DEF

bool MarniGL_LoadFunctions(void* (*getProc)(const char*))
{
    if (getProc == nullptr) return false;

    int missing = 0;
#define RE1_GL_LOAD(type, name)                                                \
    name = (type)getProc(#name);                                               \
    if (name == nullptr) {                                                     \
        fprintf(stderr, "[GL] missing entry point: %s\n", #name);              \
        ++missing;                                                             \
    }
    RE1_GL_FUNCS(RE1_GL_LOAD)
#undef RE1_GL_LOAD

    return missing == 0;
}
