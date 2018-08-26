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
  _snwprintf_s(PipeNameBuffer, MAX_PATH, L"\\\\.\\Pipe\\Exile.%08x.%08x",
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

std::vector<std::wstring_view> HttpProcess::EmptyEnvs;

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
      cmdline.assign(std::move(mexe)).append(L" ");
    }
  } else {
    if (path.find(L' ') != std::wstring_view::npos) {
      cmdline.assign(L"\"").append(path).append(L"\" ");
    } else {
      cmdline.assign(path).append(L" ");
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

  // Build Environment.
  Env env;
  DWORD createflags = 0;
  wchar_t *envptr = nullptr;
  if (!envs.empty()) {
    env.Initialize();
    for (const auto &e : envs) {
      auto np = e.find(L'=');
      if (np == std::wstring_view::npos) {
        env.SetEnv(std::wstring(e), L"", true);
      } else {
        env.SetEnv(std::wstring(e.substr(0, np)),
                   std::wstring(e.substr(np + 1)), true);
      }
    }
    createflags = CREATE_UNICODE_ENVIRONMENT;
    envptr = env.DuplicateEnv();
  }
  /// create pipes.
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  HANDLE hPipeOutputRead = NULL;
  HANDLE hPipeOutputWrite = NULL;
  HANDLE hPipeInputRead = NULL;
  HANDLE hPipeInputWrite = NULL;
  if (!KiCreatePipeEx(&hPipeOutputRead, &hPipeOutputWrite, &sa)) {
    return false;
  }
  if (!KiCreatePipeEx(&hPipeInputRead, &hPipeInputWrite, &sa)) {
    CloseHandle(hPipeOutputRead);
    CloseHandle(hPipeOutputWrite);
    return false;
  }
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  GetStartupInfoW(&si);
  si.dwFlags = STARTF_USESHOWWINDOW |
               STARTF_USESTDHANDLES; // use hStdInput hStdOutput hStdError
  si.wShowWindow = SW_HIDE;
  si.hStdInput = hPipeInputRead;
  si.hStdOutput = hPipeOutputWrite;
  /*
  BOOL
WINAPI
CreateProcessW(
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation
    );
  */

  if (CreateProcessW(NULL,        // lpApplicationName
                     &cmdline[0], // lpCommandLine
                     NULL,        // lpProcessAttributes
                     NULL,        // lpThreadAttributes
                     TRUE,        // bInheritHandles
                     createflags, // dwCreationFlags
                     envptr,      // lpEnvironment
                     NULL,        // lpCurrentDirectory
                     &si, &pi) != TRUE) {
    CloseHandle(hPipeOutputRead);
    CloseHandle(hPipeInputWrite);
    CloseHandle(hPipeOutputWrite);
    CloseHandle(hPipeInputRead);
    return false;
  }
  id = GetProcessId(pi.hProcess);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(hPipeOutputWrite);
  CloseHandle(hPipeInputRead);
  net::error_code e;
  output_.assign(hPipeOutputRead, e);
  input_.assign(hPipeInputWrite, e);
  return true;
}