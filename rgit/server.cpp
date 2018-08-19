///
#include "pch.h"
#include "server.hpp"
#include "session.hpp"

bool HttpServer::Initialize(const std::wstring &url) {
  listener = std::make_shared<net::listener_t>(url);
  if (!listener) {
    return false;
  }
  listener->support(
      net::methods::GET,
      std::bind(HttpSession::HttpHandleGet, &context, std::placeholders::_1));
  listener->support(
      net::methods::POST,
      std::bind(HttpSession::HttpHandlePost, &context, std::placeholders::_1));
  // listener->support();
  return true;
}