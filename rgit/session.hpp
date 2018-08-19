#ifndef SESSION_HPP
#define SESSION_HPP
#pragma once
#include "pch.h"
#include <memory>
#include <cpprest/http_listener.h>
#include <cpprest/details/http_helpers.h>
#include "process.hpp"
#include "context.hpp"

namespace net {
using namespace web;
using namespace http;
using namespace utility;
using namespace client;
using namespace http::experimental::listener;
} // namespace net

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
  HttpSession(const HttpSession &) = delete;
  HttpSession &operator=(const HttpSession &) = delete;
  HttpSession(HttpContext *ctx, net::http_request msg_)
      : context(ctx), msg(msg_) {
    //
  }
  static void HttpHandlePost(HttpContext *ctx, net::http_request msg);
  static void HttpHandleGet(HttpContext *ctx, net::http_request msg);

private:
  HttpContext *context;
  net::http_request msg;
  net::http_response resp;
  std::unique_ptr<web::http::details::compression::stream_decompressor>
      decompressor_;
  std::shared_ptr<HttpProcess> process;
};

#endif
