
#include "pch.h"
#include <iostream>
#include "env.hpp"
#include "server.hpp"

typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
BOOL IsRunOnWin64() {
  BOOL bIsWow64 = FALSE;
  LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
      GetModuleHandleW(L"kernel32"), "IsWow64Process");
  if (NULL != fnIsWow64Process) {
    if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64)) {
      // handle error
    }
  }
  return bIsWow64;
}

bool SearchLocation(std::wstring_view key, std::wstring_view keyitem,
                    std::wstring &location) {
  // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Git_is1
  // InstallLocation
  HKEY hInst = nullptr;
  LSTATUS result = ERROR_SUCCESS;

  WCHAR buffer[4096] = {0};
#if defined(_M_X64)
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.data(), 0, KEY_READ, &hInst) !=
      ERROR_SUCCESS) {
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.data(), 0,
                      KEY_READ | KEY_WOW64_32KEY, &hInst) != ERROR_SUCCESS) {
      // Cannot found msysgit or Git for Windows install
      return false;
    }
  }
#else
  if (IsRunOnWin64()) {
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.data(), 0,
                      KEY_READ | KEY_WOW64_64KEY, &hInst) != ERROR_SUCCESS) {
      if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.data(), 0, KEY_READ, &hInst) !=
          ERROR_SUCCESS) {
        // Cannot found msysgit or Git for Windows install
        return false;
      }
    }
  } else {
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.data(), 0, KEY_READ, &hInst) !=
        ERROR_SUCCESS) {
      return false;
    }
  }
#endif
  DWORD type = 0;
  DWORD dwSize = 4096 * sizeof(wchar_t);
  result = RegGetValueW(hInst, nullptr, keyitem.data(), RRF_RT_REG_SZ, &type,
                        buffer, &dwSize);
  if (result == ERROR_SUCCESS) {
    location.assign(buffer);
  }
  RegCloseKey(hInst);
  return result == ERROR_SUCCESS;
}

inline bool PathExeExists(const std::wstring &path) {
  auto attr = GetFileAttributesW(path.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    return false;
  }
  return true;
}

bool InitializeGitenv(std::wstring &gitexe) {
  if (SearchPathEx(L"git.exe", gitexe)) {
    wprintf(L"Find Git in PATH: %s\n", gitexe.c_str());
    return true;
  }
  constexpr const wchar_t *git4win =
      LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Git_is1)";
  constexpr const wchar_t *installKey = L"InstallLocation";

  std::wstring location;
  if (!SearchLocation(git4win, installKey, location)) {
    return false;
  }
  location.append(L"bin");
  gitexe.assign(location).append(L"\\git.exe");
  if (!PathExeExists(gitexe)) {
    gitexe.clear();
    return false;
  }
  wprintf(L"Find Git in InstallLocation: %s\n", gitexe.c_str());
  Envcontext::Instance().FlushAppendEnv(L"PATH", location.c_str(), L';');
  return true;
}

std::unique_ptr<HttpServer> httpServer;

bool initialize(const std::wstring &url, const std::wstring &gitexe,
                const std::wstring &root) {
  //
  httpServer = std::make_unique<HttpServer>();
  if (!httpServer->Initialize(url)) {
    return false;
  }
  return httpServer->Update(gitexe, root);
}

int wmain(int argc, wchar_t **argv) {
  std::cout << "Hello World!\n";
  setlocale(LC_ALL, "");
  std::wstring gitexe;
  if (!InitializeGitenv(gitexe)) {
    wprintf(L"NOT FOUND Git EXE\n");
  }
  if (!initialize(L"http://localhost:10240", gitexe, L"F:\\Github")) {
    wprintf(L"Initialize HttpServer failed\n");
    return 1;
  }
  try {
    httpServer->open().wait();
  } catch (const std::exception &e) {
    fprintf(stderr, "Open Http Server Error: %s\n", e.what());
	return 1;
  }

  httpServer->Activating(); /// run awalys
  httpServer->close().wait();
  return 0;
}
