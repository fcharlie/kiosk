// pch.cpp: 与预编译标头对应的源文件；编译成功所必需的

#include "pch.h"


#ifdef _DEBUG
#pragma comment(lib, "zlibd")
#pragma comment(lib, "cpprest_2_10d")
#else
#pragma comment(lib, "zlib")
#pragma comment(lib, "cpprest_2_10")
#endif
// 一般情况下，忽略此文件，但如果你使用的是预编译标头，请保留它。
