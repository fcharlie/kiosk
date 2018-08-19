#ifndef SERVER_HPP
#define SERVER_HPP
#pragma once
#include "pch.h"
#include <string>
#include <memory>
#include <cpprest/http_listener.h>
#include "context.hpp"

namespace net {
using namespace web;
using namespace http;
using namespace utility;
using namespace client;
using namespace http::experimental::listener;
using listener_t = web::http::experimental::listener::http_listener;
} // namespace net

class HttpServer {
public:
  HttpServer() = default;
  HttpServer(const HttpServer &) = delete;
  HttpServer &operator=(const HttpServer &) = delete;
  bool Initialize(const std::wstring &url);
  pplx::task<void> open() { return listener->open(); }
  pplx::task<void> close() { return listener->close(); }

  bool Update(const std::wstring &gitexe, const std::wstring &root) {
    return context.Initialize(gitexe, root);
  }
  void Activating() {
    context.Active();
    context.context().run();
  }

private:
  std::shared_ptr<net::listener_t> listener;
  HttpContext context;
};

#endif
