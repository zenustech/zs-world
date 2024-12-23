#pragma once

// https://abseil.io/docs/cpp/platforms/macros

#ifndef ZS_WORLD_EXPORT
#  if defined(_MSC_VER) || defined(__CYGWIN__)
#    ifdef Zs_World_EXPORT
#      define ZS_WORLD_EXPORT __declspec(dllexport)
#    else
#      define ZS_WORLD_EXPORT __declspec(dllimport)
#    endif
#  elif defined(__clang__) || defined(__GNUC__)
#    define ZS_WORLD_EXPORT __attribute__((visibility("default")))
#  endif
#endif
