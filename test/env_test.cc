///
#include <vector>
#include <string_view>
#include "../rgit/env.hpp"

bool HttpProcessExecute(std::wstring_view path,
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
    // fwrite(envptr, 1, env.EnvSize(), stderr);
    fwprintf(stderr, L"Env size: %d\n", (int)env.EnvSize());
  }
  /// create pipes.
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = FALSE;
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
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
  fwprintf(stderr, L"cmdline: %s\n", cmdline.c_str());
  if (CreateProcessW(NULL,        // lpApplicationName
                     &cmdline[0], // lpCommandLine
                     NULL,        // lpProcessAttributes
                     NULL,        // lpThreadAttributes
                     TRUE,        // bInheritHandles
                     createflags, // dwCreationFlags
                     envptr,      // lpEnvironment
                     NULL,        // lpCurrentDirectory
                     &si, &pi) != TRUE) {
    fprintf(stderr, "GetLastError: %d\n", GetLastError());
    return false;
  }
  // id = GetProcessId(pi.hProcess);
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

int wmain(int argc, wchar_t **argv) {
  //
  try {
    std::vector<std::wstring_view> Args;
    for (int i = 0; i < argc; i++) {
      Args.push_back(argv[i]);
    }
    std::vector<std::wstring_view> Envs;
    Envs.push_back(L"GL_ID=key-5589");
    HttpProcessExecute(L"cmd.exe", Args, Envs);
  } catch (const std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
  }
  return 0;
}