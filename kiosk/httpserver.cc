///
#include "httpserver.hpp"
#include "precomp.hpp"

/*
 * What's New for Windows Sockets
 * https://msdn.microsoft.com/library/windows/desktop/ms740642(v=vs.85).aspx
 * Registered Input/Output (RIO) API Extensions
 * https://technet.microsoft.com/en-us/library/hh997032.aspx
 * Mswsock.h Mswsock.lib Mswsock.dll Windows 8/ Windows Server 2012 or Later
 * https://msdn.microsoft.com/en-us/library/windows/desktop/hh448841(v=vs.85).aspx
 * RIOCreateCompletionQueue
 * http://www.cnblogs.com/gaochundong/p/csharp_tcp_service_models.html
 */

/*
 * HTTP2 Allow http.h HTTP_REQUEST_FLAG_HTTP2
 * httpserv.h:1093:// Add the SPDY/3 & HTTP/2.0 Push-Promise support
 */

#define INITIALIZE_HTTP_RESPONSE(resp, status, reason)                         \
  do {                                                                         \
    RtlZeroMemory((resp), sizeof(*(resp)));                                    \
    (resp)->StatusCode = (status);                                             \
    (resp)->pReason = (reason);                                                \
    (resp)->ReasonLength = (USHORT)strlen(reason);                             \
  } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)                         \
  do {                                                                         \
    (Response).Headers.KnownHeaders[(HeaderId)].pRawValue = (RawValue);        \
    (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength =               \
        (USHORT)strlen(RawValue);                                              \
  } while (FALSE)

template <class T> inline T *Allocate(size_t n) {
  return reinterpret_cast<T *>(HeapAlloc(GetProcessHeap(), 0, n * sizeof(T)));
}

#define MemoryFree(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

HttpServer::~HttpServer() {
  if (!HTTP_IS_NULL_ID(&urlGroupId)) {
    HttpRemoveUrlFromUrlGroup(urlGroupId, NULL, HTTP_URL_FLAG_REMOVE_ALL);
    HttpCloseUrlGroup(urlGroupId);
  }
  if (hRequestQueue) {
    HttpCloseRequestQueue(hRequestQueue);
  }
  if (!HTTP_IS_NULL_ID(&serverSessionId)) {
    HttpCloseServerSession(serverSessionId);
  }
  HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
}

VOID CALLBACK IoCompletionCallback(PTP_CALLBACK_INSTANCE Instance,
                                   PVOID pContext, PVOID pOverlapped,
                                   ULONG IoResult,
                                   ULONG_PTR NumberOfBytesTransferred,
                                   PTP_IO Io) {
  HttpServer *pServer;

  UNREFERENCED_PARAMETER(NumberOfBytesTransferred);
  UNREFERENCED_PARAMETER(Instance);
  UNREFERENCED_PARAMETER(pContext);

  HttpContext *pIoContext = reinterpret_cast<HttpContext *>(
      CONTAINING_RECORD(pOverlapped, HttpContext, Overlapped));

  pServer = pIoContext->pServer;

  pIoContext->pfCompletionFunction(pIoContext, Io, IoResult);
}

bool HttpServer::Initialize(const std::wstring &url) {
  //
  HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_2;
  if (HttpInitialize(HttpApiVersion, HTTP_INITIALIZE_SERVER, nullptr) !=
      NO_ERROR) {
    return false;
  }
  IsHttpInitialized = true;
  if (HttpCreateServerSession(HttpApiVersion, &serverSessionId, 0) !=
      NO_ERROR) {
    return false;
  }
  if (HttpCreateUrlGroup(serverSessionId, &urlGroupId, 0) != NO_ERROR) {
    return false;
  }

  // See MSDN
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364698(v=vs.85).aspx

  if (HttpAddUrlToUrlGroup(urlGroupId, url.data(), (HTTP_URL_CONTEXT)NULL, 0) !=
      NO_ERROR) {
    return false;
  }
  if (HttpCreateRequestQueue(HTTPAPI_VERSION_2, L"Kiosk.Http.Server", nullptr,
                             0, &hRequestQueue) != NO_ERROR) {
    return false;
  }
  HTTP_BINDING_INFO HttpBindingInfo = {1, hRequestQueue};

  if (HttpSetUrlGroupProperty(urlGroupId, HttpServerBindingProperty,
                              &HttpBindingInfo,
                              sizeof(HttpBindingInfo)) != NO_ERROR) {
    return false;
  }

  HTTP_TIMEOUT_LIMIT_INFO CGTimeout;
  ZeroMemory(&CGTimeout, sizeof(HTTP_TIMEOUT_LIMIT_INFO));
  CGTimeout.Flags.Present =
      1; // Specifies that the property is present on UrlGroup
  CGTimeout.EntityBody = 50; // The timeout is in secs
  //
  if (HttpSetUrlGroupProperty(urlGroupId, HttpServerTimeoutsProperty,
                              &CGTimeout,
                              sizeof(HTTP_TIMEOUT_LIMIT_INFO)) != NO_ERROR) {
    return false;
  }
  Io =
      CreateThreadpoolIo(hRequestQueue, IoCompletionCallback, nullptr, nullptr);
  if (Io == nullptr) {
    return false;
  }
  return true;
}

void HttpServer::Run() {
  // run http server
  DWORD_PTR dwProcessAffinityMask, dwSystemAffinityMask;
  WORD wRequestsCounter;
  BOOL bGetProcessAffinityMaskSucceed;

  bGetProcessAffinityMaskSucceed = GetProcessAffinityMask(
      GetCurrentProcess(), &dwProcessAffinityMask, &dwSystemAffinityMask);

  if (bGetProcessAffinityMaskSucceed) {
    for (wRequestsCounter = 0; dwProcessAffinityMask;
         dwProcessAffinityMask >>= 1) {
      if (dwProcessAffinityMask & 0x1)
        wRequestsCounter++;
    }

    wRequestsCounter = REQUESTS_PER_PROCESSOR * wRequestsCounter;
  } else {
    fprintf(stderr,
            "We could not calculate the number of processor's, "
            "the server will continue with the default number = %d\n",
            OUTSTANDING_REQUESTS);
    wRequestsCounter = OUTSTANDING_REQUESTS;
  }
  for (; wRequestsCounter > 0; --wRequestsCounter) {
    StartThreadpoolIo(Io);
	// https://docs.microsoft.com/en-us/windows/desktop/api/http/nf-http-httpreceivehttprequest
	// HttpReceiveHttpRequest
  }
}

void ReceiveCompletionCallback(
                               HttpContext *pIoContext,
                               PTP_IO Io,
                               ULONG IoResult
                               ){
}