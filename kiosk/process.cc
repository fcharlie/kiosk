////
#include "precomp.hpp"
#include <string>
#include <PathCch.h>
#include "process.hpp"

constexpr size_t kBufferSize = 64 * 1024;

/// This function only support C++17
template <class IntegerT>
[[nodiscard]] inline std::wstring
Integer_to_chars(const IntegerT _Raw_value,
                 const int _Base) noexcept // strengthened
{

  using _Unsigned = std::make_unsigned_t<IntegerT>;
  _Unsigned _Value = static_cast<_Unsigned>(_Raw_value);
  std::wstring result;
  if constexpr (std::is_signed_v<IntegerT>) {
    if (_Raw_value < 0) {
      result.push_back('-');
      _Value = static_cast<_Unsigned>(0 - _Value);
    }
  }

  constexpr size_t _Buff_size =
      sizeof(_Unsigned) * CHAR_BIT; // enough for base 2
  wchar_t _Buff[_Buff_size];
  wchar_t *const _Buff_end = _Buff + _Buff_size;
  wchar_t *_RNext = _Buff_end;

  static constexpr wchar_t _Digits[] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
      'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
      'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
  static_assert(std::size(_Digits) == 36);

  switch (_Base) {
  case 10: { // Derived from _UIntegral_to_buff()
    constexpr bool _Use_chunks = sizeof(_Unsigned) > sizeof(size_t);

    if constexpr (_Use_chunks) { // For 64-bit numbers on 32-bit platforms,
                                 // work in chunks to avoid 64-bit divisions.
      while (_Value > 0xFFFF'FFFFU) {
        unsigned long _Chunk =
            static_cast<unsigned long>(_Value % 1'000'000'000);
        _Value = static_cast<_Unsigned>(_Value / 1'000'000'000);

        for (int _Idx = 0; _Idx != 9; ++_Idx) {
          *--_RNext = static_cast<char>('0' + _Chunk % 10);
          _Chunk /= 10;
        }
      }
    }

    using _Truncated =
        std::conditional_t<_Use_chunks, unsigned long, _Unsigned>;

    _Truncated _Trunc = static_cast<_Truncated>(_Value);

    do {
      *--_RNext = static_cast<wchar_t>('0' + _Trunc % 10);
      _Trunc /= 10;
    } while (_Trunc != 0);
    break;
  }

  case 2:
    do {
      *--_RNext = static_cast<wchar_t>('0' + (_Value & 0b1));
      _Value >>= 1;
    } while (_Value != 0);
    break;

  case 4:
    do {
      *--_RNext = static_cast<wchar_t>('0' + (_Value & 0b11));
      _Value >>= 2;
    } while (_Value != 0);
    break;

  case 8:
    do {
      *--_RNext = static_cast<wchar_t>('0' + (_Value & 0b111));
      _Value >>= 3;
    } while (_Value != 0);
    break;

  case 16:
    do {
      *--_RNext = _Digits[_Value & 0b1111];
      _Value >>= 4;
    } while (_Value != 0);
    break;

  case 32:
    do {
      *--_RNext = _Digits[_Value & 0b11111];
      _Value >>= 5;
    } while (_Value != 0);
    break;

  default:
    do {
      *--_RNext = _Digits[_Value % _Base];
      _Value = static_cast<_Unsigned>(_Value / _Base);
    } while (_Value != 0);
    break;
  }

  const ptrdiff_t _Digits_written = _Buff_end - _RNext;

  result.append(_RNext, _Digits_written);
  return result;
}

bool PipeNew(OUT LPHANDLE lpReadPipe, OUT LPHANDLE lpWritePipe,
             IN LPSECURITY_ATTRIBUTES lpPipeAttributes) {
  static LONG ProcessPipeId{0};
  HANDLE ReadPipeHandle, WritePipeHandle;
  DWORD dwError;
  auto PipeId = InterlockedIncrement(&ProcessPipeId);

  auto PipeName = std::wstring(L"\\\\?\\Pipe\\Kw\\Kiosk.")
                      .append(Integer_to_chars(GetCurrentProcessId(), 16))
                      .append(L".")
                      .append(Integer_to_chars(PipeId, 16));

  ReadPipeHandle = CreateNamedPipeW(
      PipeName.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      1,           // Number of pipes
      kBufferSize, // Out buffer size
      kBufferSize, // In buffer size
      0,           //
      lpPipeAttributes);

  if (!ReadPipeHandle) {
    return false;
  }
  /// FILE_FLAG_OVERLAPPED child process input output is wait
  WritePipeHandle = CreateFileW(PipeName.c_str(), GENERIC_WRITE,
                                0, // No sharing
                                lpPipeAttributes, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                NULL // Template file
  );

  if (INVALID_HANDLE_VALUE == WritePipeHandle) {
    dwError = GetLastError();
    CloseHandle(ReadPipeHandle);
    SetLastError(dwError);
    return false;
  }

  *lpReadPipe = ReadPipeHandle;
  *lpWritePipe = WritePipeHandle;
  return true;
}

std::vector<std::wstring> SplitList(const std::wstring &s, wchar_t ch) {
  std::vector<std::wstring> sv;
  std::wstring l;
  for (auto c : s) {
    if (c == ch) {
      if (!l.empty()) {
        sv.push_back(std::move(l));
      }
      continue;
    }
    l.push_back(c);
  }
  if (!l.empty()) {
    sv.push_back(std::move(l));
  }
  return sv;
}

inline bool PathExeExists(const std::wstring &path, std::wstring_view cmd,
                          std::wstring &fullpath) {
  std::wstring fp;
  fp.assign(path).append(L"\\").append(cmd);
  auto attr = GetFileAttributesW(fp.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    return false;
  }
  fullpath.assign(std::move(fp));
  return true;
}

inline bool EndsWith(std::wstring_view r, std::wstring_view s) {
  if (r.size() < s.size()) {
    return false;
  }
  return r.compare(r.size() - s.size(), std::wstring_view::npos, s);
}

inline bool ExtensionWith(const std::vector<std::wstring> &exts,
                          std::wstring_view filename) {
  for (const auto &e : exts) {
    if (EndsWith(filename, e)) {
      return true;
    }
  }
  return false;
}

bool SearchPathEx(std::wstring_view cmd, std::wstring &fullpath) {
  auto paths = SplitList(GetEnv(L"PATH"), L';');
  auto exts = SplitList(GetEnv(L"PATHEXT"), L';');
  if (exts.empty()) {
    exts = {L".exe", L".com", L".bat", L".cmd"};
  }
  /// Current PATH
  if (PathExeExists(L".", cmd, fullpath)) {
    return true;
  }
  for (const auto &e : exts) {
    if (PathExeExists(L".", std::wstring(cmd).append(e), fullpath)) {
      return true;
    }
  }

  /// NOT LIKE CMD, When exe not endswith PATHEXT, CreateProcess can run it.
  for (const auto &p : paths) {
    if (PathExeExists(p, cmd, fullpath)) {
      return true;
    }
    for (const auto &e : exts) {
      if (PathExeExists(p, std::wstring(cmd).append(e), fullpath)) {
        return true;
      }
    }
  }
  return false;
}

int Process::Execute(std::wstring_view path, std::wstring_view args,
                     const wchar_t *env, const wchar_t *cwd) {
  //
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
  cmdline.append(args); /// build cmdline
  // CreateProcessW()

  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  HANDLE hPipeOutputRead = NULL;
  HANDLE hPipeOutputWrite = NULL;
  HANDLE hPipeInputRead = NULL;
  HANDLE hPipeInputWrite = NULL;
  if (!PipeNew(&hPipeOutputRead, &hPipeOutputWrite, &sa)) {
    return 1;
  }
  if (!PipeNew(&hPipeInputRead, &hPipeInputWrite, &sa)) {
    CloseHandle(hPipeOutputRead);
    CloseHandle(hPipeOutputWrite);
    return 1;
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
  // si.hStdError = hPipeOutputWrite;
  /// CreateProcess bInheritHandles require TRUE
  if (!CreateProcessW(NULL, &cmdline[0], NULL, NULL, TRUE,
                      (env == nullptr ? 0 : CREATE_UNICODE_ENVIRONMENT),
                      const_cast<wchar_t *>(env), cwd, &si, &pi)) {
    CloseHandle(hPipeOutputRead);
    CloseHandle(hPipeInputWrite);
    CloseHandle(hPipeOutputWrite);
    CloseHandle(hPipeInputRead);
    return 1;
  }
  ProcessId = GetProcessId(pi.hProcess);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(hPipeOutputWrite);
  CloseHandle(hPipeInputRead);
  io.Assgin(hPipeInputWrite, hPipeOutputRead);
  return 0;
}