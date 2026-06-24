#pragma once

// Lib/Dll switch
#if !defined(NEMESIS_EXPORTS) && !defined(NEMESIS_IMPORTS) && !defined(NEMESIS_STATIC)
#define NEMESIS_STATIC
#endif

#if defined(_MSC_VER)

    #ifndef COMPILER_MSVC
        #define COMPILER_MSVC 1
    #endif

    #if defined(NEMESIS_IMPORTS)
        #define NEMESIS_API __declspec(dllimport)
    #elif defined(NEMESIS_EXPORTS)
        #define NEMESIS_API __declspec(dllexport)
    #else
        #define NEMESIS_API
    #endif

#elif defined(__GNUC__)
    #define COMPILER_GCC
    #define NEMESIS_API
#else
    #error "Unknown or unsupported compiler"
#endif

// No IA64 support
#if defined (_M_AMD64) || defined (__x86_64__)
    #define USE64
#elif defined (_M_IX86) || defined (__i386__)
    #define USE32
#else
    #error "Unknown or unsupported platform"
#endif


