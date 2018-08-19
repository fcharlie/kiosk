#ifndef HTTPSERVER_HPP
#define HTTPSERVER_HPP
#pragma once
#include "precomp.hpp"
#include <Threadpoolapiset.h >
#include <functional>

// https://docs.microsoft.com/zh-cn/windows/desktop/Http/http-api-start-page
namespace kiosk {
constexpr size_t ChunkSize = 64 * 1024;
}

constexpr auto REQUESTS_PER_PROCESSOR = 4;
constexpr auto OUTSTANDING_REQUESTS = 16;

class HttpServer;
struct HttpContext;

typedef void (*HTTP_COMPLETION_FUNCTION)(struct HttpContext *, PTP_IO, ULONG);

struct HttpContext {
  OVERLAPPED Overlapped;
  HTTP_COMPLETION_FUNCTION pfCompletionFunction;
  HttpServer *pServer;
};

void ReceiveCompletionCallback(HttpContext *pIoContext, PTP_IO Io,
                               ULONG IoResult);

struct HttpRequest {
  static constexpr size_t kBufferSize = 4096;
  HttpContext ioContext;
  PHTTP_REQUEST pHttpRequest;
  UCHAR RequestBuffer[kBufferSize];
  static HttpRequest *New(HttpServer *pServer) {
    auto p = HeapAllocExZero<HttpRequest>();
    p->ioContext.pServer = pServer;
    p->ioContext.pfCompletionFunction = ReceiveCompletionCallback;
	 p->pHttpRequest = (PHTTP_REQUEST) p->RequestBuffer;
    return nullptr;
  }
};

struct HttpResponse {
  HttpContext ioContext;
  HTTP_RESPONSE Response;
  HTTP_DATA_CHUNK HttpDataChunk;
};

class HttpServer {
public:
  HttpServer() = default;
  HttpServer(const HttpServer &) = delete;
  HttpServer &operator=(const HttpServer &) = delete;
  ~HttpServer();
  bool Initialize(const std::wstring &url);
  void Run();

private:
  HTTP_SERVER_SESSION_ID serverSessionId{HTTP_NULL_ID};
  HTTP_URL_GROUP_ID urlGroupId = HTTP_NULL_ID;
  HANDLE hRequestQueue{INVALID_HANDLE_VALUE};
  PTP_IO Io;
  bool IsHttpInitialized{false};
  bool IsStopped{false};
};

#endif
