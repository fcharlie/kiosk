
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
//
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Mswsock.h>
#include <stdio.h>

#pragma comment(lib,"Ws2_32.lib")

// http://www.serverframework.com/asynchronousevents/2011/10/windows-8-registered-io-networking-extensions.html

int wmain(int argc, wchar_t **argv) {
  //

  SOCKET s = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
                          WSA_FLAG_REGISTERED_IO);
  RIO_EXTENSION_FUNCTION_TABLE rio;
  GUID functionTableId = WSAID_MULTIPLE_RIO;

  DWORD dwBytes = 0;

  bool ok = true;

  if (0 != WSAIoctl(s, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
                    &functionTableId, sizeof(GUID), (void **)&rio, sizeof(rio),
                    &dwBytes, 0, 0)) {
    const DWORD lastError = ::GetLastError();

    // handle error...
  } else {
    // all ok, we have access to RIO
  }
  return 0;
}