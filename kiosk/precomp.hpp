#ifndef PRECOMP_HPP
#define PRECOMP_HPP

#pragma once
#ifndef UNICODE
#define UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <http.h>
#include <stdio.h>

template <typename T> inline T *HeapAllocExZero(size_t count = 1) {
  auto p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(T) * count);
  return static_cast<T *>(p);
}

template <typename T> inline T *HeapAllocEx(size_t count = 1) {
  auto p = HeapAlloc(GetProcessHeap(), 0, sizeof(T) * count);
  return static_cast<T *>(p);
}

inline void HeapFreeEx(LPVOID m) {
  //
  HeapFree(GetProcessHeap(), 0, m);
}

#pragma comment(lib, "httpapi.lib")

#endif
