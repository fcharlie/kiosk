#ifndef PROCESS_HPP
#define PROCESS_HPP
#include "precomp.hpp"
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#pragma once

// Hash Impl
struct CaseInsensitiveHash {
  std::size_t operator()(const std::wstring &Keyval) const noexcept {
    /// See Wikipedia
    /// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
#if defined(__x86_64) || defined(_WIN64)
    static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
    size_t _Val = 14695981039346656037ULL;
    constexpr size_t _FNV_prime = 1099511628211ULL;
#else
    static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
    size_t _Val = 2166136261U;
    constexpr size_t _FNV_prime = 16777619U;
#endif

    for (auto c : Keyval) {
      wchar_t _Ch = static_cast<wchar_t>(towlower(c));
      _Val ^= static_cast<size_t>(_Ch & 0xFF);
      _Val *= _FNV_prime;
      _Val ^= static_cast<size_t>(_Ch >> 8);
      _Val *= _FNV_prime;
    }
    return _Val;
  }
};

struct CaseInsensitiveEqual {
  bool operator()(const std::wstring &lstr, const std::wstring &rstr) const {
    size_t len;
    if ((len = lstr.size()) != rstr.size()) {
      return false;
    }
    auto l = lstr.data();
    auto r = rstr.data();
    for (size_t i = 0; i < len; i++) {
      if (towlower(l[i]) != towlower(r[i])) {
        return false;
      }
    }
    return true;
  }
};

using Envcontainer =
    std::unordered_map<std::wstring, std::wstring, CaseInsensitiveHash,
                       CaseInsensitiveEqual>;

class Envcontext {
public:
  struct EnvMutex {
    EnvMutex(LPCRITICAL_SECTION lpc) {
      lpSection = lpc;
      EnterCriticalSection(lpSection);
    }
    ~EnvMutex() {
      //
      LeaveCriticalSection(lpSection);
    }
    LPCRITICAL_SECTION lpSection{nullptr};
  };
  Envcontext(const Envcontext &) = delete;
  Envcontext &operator=(const Envcontext &) = delete;
  ~Envcontext() {
    ///
    DeleteCriticalSection(&section);
  }
  static Envcontext &Instance() {
    static Envcontext ctx;
    return ctx;
  }
  bool SetEnv(const wchar_t *key, const wchar_t *value, bool overwrite) {
    EnvMutex mtx(&section);
    auto it = menv.find(key);
    if (it != menv.end() && !overwrite) {
      return false;
    }
    menv.insert(std::pair<std::wstring, std::wstring>(key, value));
    return true;
  }
  bool AppendEnv(const wchar_t *key, const wchar_t *addval, wchar_t ch) {
    auto p = GetEnv(key);
    if (p.empty()) {
      return SetEnv(key, addval, true);
    }
    if (p.back() != ch) {
      p.push_back(ch);
    }
    p.append(addval);
    return SetEnv(key, p.data(), true);
  }

  bool FlushSetEnv(const wchar_t *key, const wchar_t *value, bool overwrite) {
    if (!SetEnv(key, value, overwrite)) {
      return false;
    }
    return SetEnvironmentVariableW(key, value) == TRUE;
  }
  //
  bool FlushAppendEnv(const wchar_t *key, const wchar_t *addval, wchar_t ch) {
    auto p = GetEnv(key);
    if (p.empty()) {
      return FlushSetEnv(key, addval, true);
    }
    if (p.back() != ch) {
      p.push_back(ch);
    }
    p.append(addval);
    return FlushSetEnv(key, p.data(), true);
  }
  std::wstring GetEnv(const wchar_t *key) {
    EnvMutex mtx(&section);
    auto it = menv.find(key);
    if (it == menv.end()) {
      return L"";
    }
    return it->second;
  }
  bool UnSetEnv(const wchar_t *key) {
    EnvMutex mtx(&section);
    auto it = menv.find(key);
    if (it == menv.end()) {
      return false;
    }
    menv.erase(it);
    return true;
  }
  const Envcontainer &MapEnv() const { return menv; }

private:
  Envcontainer menv; // Map of Env
  CRITICAL_SECTION section;
  bool InitializeEnvs() {
    EnvMutex mtx(&section);
    auto envs = GetEnvironmentStringsW();
    if (envs == nullptr) {
      return false;
    }
    for (auto it = envs; *it; it++) {
      auto l = wcslen(it);
      auto off = wmemchr(it, L'=', l);
      if (off - it < 1) {
        menv.insert(std::pair<std::wstring, std::wstring>(it, L""));
      } else {
        menv.insert(std::pair<std::wstring, std::wstring>(
            std::wstring(it, off - it), off + 1));
      }
      it += l;
    }
    FreeEnvironmentStringsW(envs);
    return true;
  }

  Envcontext() {
    //
    InitializeCriticalSection(&section); /// TODO
    if (InitializeEnvs()) {
      /// Throw errors
    }
  }
};

inline std::wstring GetEnv(const wchar_t *key) {
  return Envcontext::Instance().GetEnv(key);
}

class Env {
public:
  Env() = default;
  Env(const Env &) = delete;
  Env &operator=(const Env &) = delete;
  ~Env() {
    if (envbuf != nullptr) {
      HeapFreeEx(envbuf);
    }
  }
  Env &Initialize() {
    menvcopy = Envcontext::Instance().MapEnv();
    return *this;
  }
  Env &SetEnv(const wchar_t *key, const wchar_t *value, bool overwrite) {
    auto it = menvcopy.find(key);
    if (it != menvcopy.end() && !overwrite) {
      return *this;
    }
    menvcopy.insert(std::pair<std::wstring, std::wstring>(key, value));
    return *this;
  }
  const wchar_t *DuplicateEnv() {
    //
    if (envbuf != nullptr) {
      HeapFreeEx(envbuf);
      envbuf = nullptr;
    }
    size_t reqsize = 0;
    for (const auto &e : menvcopy) {
      reqsize += e.first.size() + 1 + e.second.size() + 1; // K=V\0
    }
    reqsize++; // append \0

    envbuf = HeapAllocEx<wchar_t>(reqsize);
    if (envbuf == nullptr) {
      // Cannot alloc
      return nullptr;
    }
    auto it = envbuf;
    for (const auto &e : menvcopy) {
      wmemcpy(it, e.first.data(), e.first.size());
      it += e.first.size();
      if (e.second.empty()) {
        *it++ = L'\0';
      }
      *it++ = L'=';
      wmemcpy(it, e.second.data(), e.second.size());
      it += e.second.size();
      *it++ = L'\0';
    }
    *it++ = L'\0';
    buflen = reqsize;
    return envbuf;
  }

private:
  Envcontainer menvcopy;
  wchar_t *envbuf{nullptr};
  size_t buflen{0};
};

class Stdio {
public:
  Stdio() = default;
  Stdio(const Stdio &) = delete;
  Stdio &operator=(const Stdio &) = delete;
  ~Stdio() {
    if (wfd != INVALID_HANDLE_VALUE) {
      CloseHandle(wfd);
    }
    if (rfd != INVALID_HANDLE_VALUE) {
      CloseHandle(rfd);
    }
  }
  bool Assgin(HANDLE hRead, HANDLE hWrite) {
    if (rfd != INVALID_HANDLE_VALUE || wfd != INVALID_HANDLE_VALUE) {
      return false;
    }
    rfd = hRead;
    wfd = hWrite;
    return true;
  }
  HANDLE Rfd() const { return rfd; }
  HANDLE Wfd() const { return wfd; }

private:
  HANDLE rfd{INVALID_HANDLE_VALUE};
  HANDLE wfd{INVALID_HANDLE_VALUE};
};

class Process {
public:
  Process() = default;
  Process(const Process &) = delete;
  Process &operator=(const Process &) = delete;

  int Execve(std::wstring_view path, std::vector<std::wstring_view> &Args,
             const wchar_t *env, const wchar_t *cwd) {
    std::wstring ws;
    for (const auto &a : Args) {
      if (a.empty()) {
        ws.append(L"\"\" ");
        continue;
      }
      if (a.find(L' ') != std::wstring_view::npos) {
        ws.append(L"\"").append(a).append(L"\"");
      } else {
        ws.append(a).append(L" ");
      }
    }
    return Execute(path, ws, env, cwd);
  }

  int Execute(std::wstring_view path, std::wstring_view cmdline,
              const wchar_t *env, const wchar_t *cwd);
  using Ssize_t = SSIZE_T;
  Ssize_t Write(const void *data, size_t bytes) {
    DWORD dwwrite = 0;
    if (!WriteFile(io.Wfd(), data, (DWORD)bytes, &dwwrite, nullptr)) {
      return -1;
    }
    return static_cast<Ssize_t>(dwwrite);
  }

private:
  DWORD ProcessId{0};
  Stdio io;
};

#endif
