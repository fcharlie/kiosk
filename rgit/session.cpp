///
#include "pch.h"
#include "session.hpp"

///

void HttpSession::HttpHandlePost(HttpContext *ctx, net::http_request msg) {
  auto session = std::make_shared<HttpSession>(ctx, msg);
}
void HttpSession::HttpHandleGet(HttpContext *ctx, net::http_request msg) {
  auto session = std::make_shared<HttpSession>(ctx, msg);
  wprintf(L"%s\n", msg.absolute_uri().path().c_str());
  msg.reply(200,"Hello world");
}
