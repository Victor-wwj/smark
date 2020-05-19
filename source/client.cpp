#include "client.h"

#include "debug.h"

namespace smark {
  TCPClient::TCPClient(smark::util::EventLoop* el) : Socket(el) {}

  HttpClient::HttpClient(smark::util::EventLoop* el) : TCPClient(el) {
    // writable_event = [this](auto) {
    //   if (!this->request_queue_.empty()) {
    //     auto req = this->request_queue_.front();
    //     auto data = req->ToString();
    //     this->cli_.Send(data.c_str(), data.length());
    //     request_queue_.pop();
    //   }
    // };
    on_read = [this](const char* data, ssize_t recved) {
      parser_.Feed(data, recved);
      if (recved != 0 && data[recved - 1] == '\0') parser_.Feed(data, 0);  // end of response
    };
    parser_.on_complete = [this](std::shared_ptr<util::HttpResponse> res) {
      if (this->on_response) this->on_response(this, res);
    };
    parser_.Init();
  }

  void HttpClient::Request(std::shared_ptr<util::HttpRequest> request) {
    auto data = request->ToString();
    this->Write(data.c_str(), data.length());
  }

}  // namespace smark
