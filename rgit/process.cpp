////////
#include "pch.h"
#include <cstdio>
#include "process.hpp"
#include "env.hpp"

LONG ProcessPipeId_ = 0;

BOOL WINAPI KiCreatePipeEx(OUT LPHANDLE lpReadPipe, OUT LPHANDLE lpWritePipe,
                           IN LPSECURITY_ATTRIBUTES lpPipeAttributes) {
  HANDLE ReadPipeHandle, WritePipeHandle;
  DWORD dwError;
  WCHAR PipeNameBuffer[MAX_PATH];

  //
  // Only one valid OpenMode flag - FILE_FLAG_OVERLAPPED
  //
  auto PipeId = InterlockedIncrement(&ProcessPipeId_);
  _snwprintf_s(PipeNameBuffer, MAX_PATH, L"\\\\.\\Pipe\\Basalt.%08x.%08x",
               GetCurrentProcessId(), PipeId);
  ReadPipeHandle = CreateNamedPipeW(
      PipeNameBuffer, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      1,     // Number of pipes
      65536, // Out buffer size
      65536, // In buffer size
      0,     //
      lpPipeAttributes);

  if (!ReadPipeHandle) {
    return FALSE;
  }

  WritePipeHandle = CreateFileW(
      PipeNameBuffer, GENERIC_WRITE,
      0, // No sharing
      lpPipeAttributes, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL |
          FILE_FLAG_OVERLAPPED, /// child process input output is wait
      NULL                      // Template file
  );

  if (INVALID_HANDLE_VALUE == WritePipeHandle) {
    dwError = GetLastError();
    CloseHandle(ReadPipeHandle);
    SetLastError(dwError);
    return FALSE;
  }

  *lpReadPipe = ReadPipeHandle;
  *lpWritePipe = WritePipeHandle;
  return TRUE;
}


std::vector<std::wstring_view> HttpProcess::empty;

bool HttpProcess::Execute(std::wstring_view path,
                          std::vector<std::wstring_view> args,
                          std::vector<std::wstring_view> envs) {
	  std::wstring cmdline;
  if (path.find(L'/') == std::wstring_view::npos &&
      path.find(L'\\') == std::wstring_view::npos) {
    std::wstring mexe;
    if (!SearchPathEx(path, mexe)) {
      return 1;
    }
    if (mexe.find(L' ') != std::wstring_view::npos) {
      cmdline.assign(L"\"").append(mexe).append(L"\" ");
    } else {
      cmdline.assign(std::move(mexe)).append(L"\" ");
    }
  } else {
    if (path.find(L' ') != std::wstring_view::npos) {
      cmdline.assign(L"\"").append(path).append(L"\" ");
    } else {
      cmdline.assign(path).append(L"\" ");
    }
  }
      for (const auto &a : args) {
      if (a.empty()) {
        cmdline.append(L"\"\" ");
        continue;
      }
      if (a.find(L' ') != std::wstring_view::npos) {
        cmdline.append(L"\"").append(a).append(L"\"");
      } else {
        cmdline.append(a).append(L" ");
      }
    }
  return true;
}