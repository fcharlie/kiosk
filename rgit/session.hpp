#ifndef SESSION_HPP
#define SESSION_HPP
#pragma once
#include "pch.h"
#include <memory>
#include <optional>
#include <cpprest/http_listener.h>
#include <cpprest/details/http_helpers.h>
#include <cpprest/filestream.h>
#include <cpprest/producerconsumerstream.h>
#include <cpprest/rawptrstream.h>
#include "process.hpp"
#include "context.hpp"

namespace net {
using namespace web;
using namespace http;
using namespace utility;
using namespace client;
using namespace concurrency;
using namespace http::experimental::listener;
using stream_decompressor =
    web::http::details::compression::stream_decompressor;
using compression_algorithm =
    web::http::details::compression::compression_algorithm;
using data_buffer = web::http::details::compression::data_buffer;
} // namespace net

class AsyncInput {
public:
private:
};

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
  HttpSession(const HttpSession &) = delete;
  HttpSession &operator=(const HttpSession &) = delete;
  HttpSession(HttpContext *ctx, net::http_request msg_)
      : context(ctx), msg(msg_) {
    //
  }

  std::optional<std::wstring> PathRepoExist(const std::wstring &rp);

  static void HttpHandlePost(HttpContext *ctx, net::http_request msg);
  static void HttpHandleGet(HttpContext *ctx, net::http_request msg);
  void HandleServiceRefs();
  void HandleServiceRpc();
  void CopyFromStdout(const net::error_code &ec, size_t byres);
  void ReceiveBody(size_t bytes);
  void RpcResponse();

private:
  HttpContext *context;
  net::http_request msg;
  net::http_response resp;
  std::unique_ptr<web::http::details::compression::stream_decompressor>
      decompressor_;
  std::shared_ptr<HttpProcess> process;
  net::streams::producer_consumer_buffer<uint8_t> outs;
  // net::streams::rawptr_buffer<uint8_t> ins;
  net::data_buffer decbuf;
  uint8_t buffer[8192];
};

#endif
