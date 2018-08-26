#ifndef PROCESS_HPP
#define PROCESS_HPP
#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <boost/asio.hpp>
#include <boost/asio/windows/stream_handle.hpp>

using ProcessId = DWORD;

namespace net {
using error_code = boost::system::error_code;
using stdiostream_t = boost::asio::windows::stream_handle;
using io_context_t = boost::asio::io_context;
using namespace boost::asio;
} // namespace net

class HttpProcess {
public:
  HttpProcess(const HttpProcess &) = delete;
  HttpProcess &operator=(const HttpProcess &) = delete;
  HttpProcess(net::io_context_t &ctx) : input_(ctx), output_(ctx) {
    //
  }
  ~HttpProcess(){
	  // TODO
	  cancel();
  }
  template <typename ConstBufferSequence, typename WriteHandler>
  void async_write_some(const ConstBufferSequence &buffers,
                        WriteHandler &&handler) {
    input_.async_write_some(buffers, handler);
  }
  //// Read
  template <typename MutableBufferSequence, typename ReadHandler>
  void async_read_some(const MutableBufferSequence &buffers,
                       ReadHandler &&handler) {
    output_.async_read_some(buffers, handler);
  }
  ////
  template <typename ConstBufferSequence, typename WriteHandler>
  void async_write(const ConstBufferSequence &buffers, WriteHandler &&handler) {
    net::async_write(input_, buffers, handler);
  }
  /////
  template <typename MutableBufferSequence, typename ReadHandler>
  void async_read(const MutableBufferSequence &buffers, ReadHandler &&handler) {
    net::async_read(output_, buffers, handler);
  }
  void cancel() {
    input_.cancel();
    output_.cancel();
  }
  void cancel(net::error_code &ec) {
    input_.cancel(ec);
    output_.cancel(ec);
  }
  void closewrite() {
    net::error_code ec;
    if (input_.is_open()) {
      input_.cancel(ec);
      input_.close(ec);
    }
  }
  bool Execute(std::wstring_view cmd, std::vector<std::wstring_view> args,
               std::vector<std::wstring_view> envs = EmptyEnvs);

private:
  net::stdiostream_t input_;
  net::stdiostream_t output_;
  ProcessId id{0};
  static std::vector<std::wstring_view> EmptyEnvs;
};

#endif