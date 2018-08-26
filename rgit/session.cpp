///
#include "pch.h"
#include "session.hpp"

///

constexpr const auto GIT_PROTOCOL_HEADER = L"Git-Protocol";
constexpr const auto GIT_PROTOCOL_ENVIRONMENT = L"GIT_PROTOCOL";

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
  auto np = context->Root() + rp; // url start with '/'
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

inline const wchar_t *servicecmd(std::wstring_view s) {
  if (s == L"git-upload-pack") {
    return L"upload-pack";
  }
  if (s == L"git-receive-pack") {
    return L"receive-pack";
  }
  return nullptr;
}

void HttpSession::HandleServiceRefs() {
  auto path = net::http::uri::decode(msg.relative_uri().path());
  if (!EndsWith(path, L"/info/refs")) {
    msg.reply(404, "Not Found");
    return;
  }
  path.resize(path.size() - sizeof("/info/refs") + 1);
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
    msg.reply(404, "Repository Not Found");
    return;
  }
  std::wstring version;
  auto ite = msg.headers().find(GIT_PROTOCOL_HEADER);
  if (ite != msg.headers().end()) {
    version = ite->second;
  }
  fwprintf_s(stderr, L"git %s --stateless-rpc --advertise-refs %s\n", cmd,
             opath->c_str());
  process = std::make_shared<HttpProcess>(context->context());
  // start process git $service --stateless-rpc --advertise-refs $gitdir
  std::vector<std::wstring_view> Args;
  /// git other flags todo
  Args.push_back(cmd);
  Args.push_back(L"--stateless-rpc");
  Args.push_back(L"--advertise-refs");
  Args.push_back(*opath); /// path
  std::vector<std::wstring_view> Envs;
  if (!version.empty()) {
    Envs.push_back(
        std::wstring(GIT_PROTOCOL_ENVIRONMENT).append(L"=").append(version));
    // Encode services to buffer
  }
  // run process
  if (!process->Execute(context->Gitexe(), Args, Envs)) {
    msg.reply(net::status_codes::InternalError, "Internal Server Error");
    return;
  }

  resp.headers().add(L"Expires", L"Fri, 01 Jan 1980 00:00:00 GMT");
  resp.headers().add(L"Pragma", L"no-cache");
  resp.headers().add(L"Cache-Control", L"no-cache, max-age=0, must-revalidate");
  resp.set_status_code(net::status_codes::OK);
  constexpr uint8_t uchunk[] = "001e# service=git-upload-pack\n0000";
  constexpr uint8_t rchunk[] = "001f# service=git-receive-pack\n0000";
  const uint8_t *chptr;
  size_t cklen;
  // net::streams::basic_istream<uint8_t> stream(outs);
  if (wcscmp(cmd, L"receive-pack") == 0) {
    resp.set_body(outs, L"application/x-git-receive-pack-advertisement");
    chptr = rchunk;
    cklen = std::size(rchunk) - 1;
  } else {
    resp.set_body(outs, L"application/x-git-upload-pack-advertisement");
    chptr = uchunk;
    cklen = std::size(uchunk) - 1;
  }
  msg.reply(resp);
  auto self(shared_from_this());
  outs.putn_nocopy(chptr, cklen).then([this, self, cklen](size_t bytes) {
    if (cklen != bytes) {
      return;
    }
    process->async_read(net::buffer(buffer),
                        std::bind(&HttpSession::CopyFromStdout,
                                  shared_from_this(), std::placeholders::_1,
                                  std::placeholders::_2));
  });
}

// Service RPC
void HttpSession::HandleServiceRpc() {
  auto path = net::http::uri::decode(msg.relative_uri().path());
  auto np = path.rfind(L'/');
  if (np == std::wstring::npos) {
    msg.reply(net::status_codes::BadRequest, "Unsupport url");
    return;
  }

  auto cmd = servicecmd({path.data() + np + 1, path.size() - np - 1});
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
  auto it = msg.headers().find(GIT_PROTOCOL_HEADER);
  if (it != msg.headers().end()) {
    version = it->second;
  }

  fwprintf_s(stderr, L"git %s --stateless-rpc %s\n", cmd, opath->c_str());
  process = std::make_shared<HttpProcess>(context->context());
  // start process git $service --stateless-rpc $gitdir
  std::vector<std::wstring_view> Args;
  /// git other flags todo
  Args.push_back(cmd);
  Args.push_back(L"--stateless-rpc");
  Args.push_back(*opath); /// path
  std::vector<std::wstring_view> Envs;
  if (!version.empty()) {
    Envs.push_back(
        std::wstring(GIT_PROTOCOL_ENVIRONMENT).append(L"=").append(version));
    // Encode services to buffer
  }
  // run process
  if (!process->Execute(context->Gitexe(), Args, Envs)) {
    msg.reply(net::status_codes::InternalError, "Internal Server Error");
    return;
  }
  auto it_ = msg.headers().find(L"Content-Encoding");
  /// Content-Encoding
  if (it_ != msg.headers().end()) {
    auto alg = net::stream_decompressor::to_compression_algorithm(it_->second);
    if (alg == net::compression_algorithm::invalid) {
      msg.reply(net::status_codes::BadRequest,
                "Unsupported compression algorithm");
      return;
    }
    decompressor_ = std::make_unique<net::stream_decompressor>(alg);
  }
  // SET no reply.
  if (wcscmp(cmd, L"receive-pack") == 0) {
    resp.set_body(outs, L"application/x-git-receive-pack-result");

  } else {
    resp.set_body(outs, L"application/x-git-upload-pack-result");
  }
  ///
  msg.body()
      .read(net::streams::rawptr_buffer(buffer, sizeof(buffer)), 8192)
      .then(std::bind(&HttpSession::ReceiveBody, shared_from_this(),
                      std::placeholders::_1));
}

void HttpSession::ReceiveBody(std::size_t bytes) {
  auto self(shared_from_this());
  if (bytes == 0) {
    /// END TODO
    auto self(shared_from_this());
    process->async_write(net::buffer("\0", 1),
                         [this, self](net::error_code ec, size_t bytes) {
                           if (ec) {
                             msg.reply(net::status_codes::InternalError);
                             return;
                           }
                           return RpcResponse();
                         });
    return;
  }

  if (decompressor_) {
    decbuf = decompressor_->decompress(buffer, bytes);
    process->async_write(
        net::buffer(decbuf),
        [self, this, bytes](net::error_code ec, size_t writes) {
          if (ec) {
            msg.reply(net::status_codes::InternalError);
            return;
          }
          if (bytes < 8192) {
            return ReceiveBody(0);
          }
          ///
          msg.body()
              .read(net::streams::rawptr_buffer(buffer, sizeof(buffer)), 8192)
              .then(std::bind(&HttpSession::ReceiveBody, shared_from_this(),
                              std::placeholders::_1));
        });
  } else {
    process->async_write(
        net::buffer(buffer, bytes),
        [self, this, bytes](net::error_code ec, size_t writes) {
          if (ec) {
            msg.reply(net::status_codes::InternalError);
            return;
          }
          if (bytes < 8192) {
            return ReceiveBody(0);
          }
          msg.body()
              .read(net::streams::rawptr_buffer(buffer, sizeof(buffer)), 8192)
              .then(std::bind(&HttpSession::ReceiveBody, shared_from_this(),
                              std::placeholders::_1));
        });
  }
}

void HttpSession::RpcResponse() {
  resp.set_body(outs);
  resp.headers().add(L"Expires", L"Fri, 01 Jan 1980 00:00:00 GMT");
  resp.headers().add(L"Pragma", L"no-cache");
  resp.headers().add(L"Cache-Control", L"no-cache, max-age=0, must-revalidate");
  resp.set_status_code(net::status_codes::OK);
  msg.reply(resp);
  process->async_read(net::buffer(buffer),
                      std::bind(&HttpSession::CopyFromStdout,
                                shared_from_this(), std::placeholders::_1,
                                std::placeholders::_2));
}

void HttpSession::CopyFromStdout(const net::error_code &ec, size_t bytes) {
  if (ec) {
    fprintf(stderr, "DEBUG: %s\n", ec.message().c_str());
    outs.close(std::ios_base::out);
    return;
  }
  // fwrite(buffer, 1, bytes, stderr);
  auto self(shared_from_this());
  outs.putn_nocopy(buffer, bytes).then([this, self, bytes](size_t p) {
    if (bytes != p) {
      outs.close(std::ios_base::out);
      return;
    }
    process->async_read(net::buffer(buffer),
                        std::bind(&HttpSession::CopyFromStdout,
                                  shared_from_this(), std::placeholders::_1,
                                  std::placeholders::_2));
    //outs.sync();
  });
}