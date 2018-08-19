#ifndef CONTEXT_HPP
#define CONTEXT_HPP
#pragma once

#include <boost/asio.hpp>

class HttpServer;
class HttpContext {
public:
  HttpContext() = default;
  HttpContext(const HttpContext &) = delete;
  HttpContext operator=(const HttpContext &) = delete;
  using io_context_t = boost::asio::io_context;
  using work_t = boost::asio::io_service::work;
  io_context_t &context() {
    // io context;
    return ctx;
  }

  void Active() {
    //
    work = std::make_shared<work_t>(ctx);
  }

  const std::wstring &Gitexe() const { return gitexe; }
  const std::wstring &Root() const { return root; }

private:
  friend class HttpServer;
  bool Initialize(const std::wstring &ge, const std::wstring &r) {
    gitexe = ge;
    root = r;
    return true;
  }
  io_context_t ctx;
  std::shared_ptr<work_t> work;
  std::wstring gitexe;
  std::wstring root;
};

#endif
