///
#include "pch.h"
#include "session.hpp"

///

constexpr const auto GIT_PROTOCOL = L"GIT_PROTOCOL";

inline bool EndsWith(std::wstring_view s, std::wstring_view suffix) {
  if (s.size() < suffix.size()) {
    return false;
  }
  return s.compare(s.size() - suffix.size(), suffix.size(), suffix.data()) == 0;
}

void renderNotFond(net::http_request msg) {
  /// 404
  msg.reply(404, "Not Found");
}

std::optional<std::wstring> HttpSession::PathRepoExist(const std::wstring &rp) {
  // TODO get path
  auto np = context->Root() + L"\\" + rp;
  auto attr = GetFileAttributesW(np.c_str());
  if (attr != INVALID_FILE_ATTRIBUTES &&
      (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    return std::make_optional<std::wstring>(np);
  }
  if (!EndsWith(np, L".git")) {
    np.append(L".git");
  }
  attr = GetFileAttributesW(np.c_str());
  if (attr != INVALID_FILE_ATTRIBUTES &&
      (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
    return std::make_optional<std::wstring>(np);
  }
  return std::nullopt;
}

/// router

void HttpSession::HttpHandlePost(HttpContext *ctx, net::http_request msg) {
  auto session = std::make_shared<HttpSession>(ctx, msg);
  session->HandleServiceRpc();
}

void HttpSession::HttpHandleGet(HttpContext *ctx, net::http_request msg) {
  auto session = std::make_shared<HttpSession>(ctx, msg);
  session->HandleServiceRefs();
}

inline const wchar_t *servicecmd(const std::wstring &s) {
  if (s == L"git-upload-pack") {
    return L"upload-pack";
  }
  if (s == L"git-receive-pack") {
    return L"receive-pack";
  }
  return nullptr;
}

void HttpSession::HandleServiceRefs() {
  auto path = net::http::uri::decode(msg.relative_uri().path);
  if (!EndsWith(path, L"/info/refs")) {
    msg.reply(404, "Not Found");
    return;
  }
  path.resize(path.size() - sizeof("/info/refs") - 1);
  auto query = net::uri::split_query(msg.relative_uri().query());
  auto it = query.find(L"service");
  if (it == query.end()) {
    msg.reply(404, "Not Found");
    return;
  }
  const wchar_t *cmd = servicecmd(it->second);
  if (cmd == nullptr) {
    msg.reply(400, "Bad Request");
    return;
  }
  auto opath = PathRepoExist(path);
  if (!opath) {
    msg.reply(404, "Not Found");
    return;
  }
  std::wstring version;
  auto it = msg.headers().find(GIT_PROTOCOL);
  if (it != msg.headers().end()) {
    version = it->second;
  }
  if (version.empty()) {
    ////
  }
  ///
  //////////////
}
void HttpSession::HandleServiceRpc() {
  auto path = net::http::uri::decode(msg.relative_uri().path);
  auto np = path.rfind(L'/');
  if (np == std::wstring::npos) {
    msg.reply(net::status_codes::BadRequest, "Unsupport url");
    return;
  }
  auto cmd = servicecmd(path.substr(np + 1));
  if (cmd == nullptr) {
    msg.reply(404, "Not Found");
    return;
  }
  path.resize(np);
  auto opath = PathRepoExist(path);
  if (!opath) {
    msg.reply(404, "Not Found");
    return;
  }
  std::wstring version;
  auto it = msg.headers().find(GIT_PROTOCOL);
  if (it != msg.headers().end()) {
    version = it->second;
  }
}

void HttpSession::WriteToStdin() {
  auto self(shared_from_this());
  msg.body().read(target, 4096).then([self, this](size_t count) {
    ///
    // auto s(shared_from_this());
    // return target.putn_nocopy(buffer, count).then([self, this]() {

    //});
  });
}

void HttpSession::CopyFromStdout(const net::error_code &ec, size_t bytes) {
  if (ec) {
    return;
  }
  auto self(shared_from_this());
  buf.putn_nocopy(buffer, bytes).then([this, self, bytes](size_t p) {
    if (bytes != p) {
      return;
    }
    process->async_read(net::buffer(buffer),
                        std::bind(&HttpSession::CopyFromStdout, shared_from_this(),
                                  std::placeholders::_1,
                                  std::placeholders::_2));
  });
}