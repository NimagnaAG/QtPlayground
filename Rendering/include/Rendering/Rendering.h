#pragma once

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the TEST_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// TEST_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  #define NIMAGNA_WINDOWS 1
  #ifdef RENDERING_EXPORTS
    #define RENDERING_API __declspec(dllexport)
  #else
    #define RENDERING_API __declspec(dllimport)
  #endif
#elif __APPLE__
  #define NIMAGNA_MACOS 1
  #define RENDERING_API
#endif

#ifdef NDEBUG
  #define NIMAGNA_RELEASE
#else
  #define NIMAGNA_DEBUG
#endif
