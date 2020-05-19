#include "util.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <tuple>

#include "debug.h"

namespace smark::util {
  EventLoop::EventLoop() {
    // #ifdef SUPPORT_AE
    //     ae_el_ = aeCreateEventLoop(set_size);
    // #endif
    uv_loop_init(loop_.get());
  }

  // void EventLoop::writable_proc(aeEventLoop *loop, int fd, void *data, int mask) {
  //   (void)loop;  // prevent unused parameter error
  //   (void)mask;
  //   auto el = reinterpret_cast<EventLoop *>(data);
  //   auto obj = el->obj_map_[fd];
  //   if (obj->writable_event) obj->writable_event(el);
  // }
  // void EventLoop::readable_proc(aeEventLoop *loop, int fd, void *data, int mask) {
  //   (void)loop;
  //   (void)mask;
  //   auto el = reinterpret_cast<EventLoop *>(data);
  //   auto obj = el->obj_map_[fd];
  //   if (obj->readable_event) obj->readable_event(el);
  // }

  void EventLoop::Wait() { uv_run(loop_.get(), UV_RUN_DEFAULT); }

  void EventLoop::Stop() { uv_stop(loop_.get()); }

  typedef struct {
    uv_write_t req;
    uv_buf_t buf;
  } write_req_t;

  void on_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    auto socket = reinterpret_cast<Socket *>(stream->data);
    DEFER(delete buf->base;)

    if (socket->on_read) {
      socket->on_read(buf->base, nread);
    }
  }

  void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    (void)handle;
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
  }

  Socket::Socket(EventLoop *loop) {
    // fd_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    // if (fd_ < 0) {
    //   ERR("Create socket fail.");
    // }
    uv_tcp_init(loop->loop_.get(), socket_.get());
    socket_.get()->data = this;
  }

  // void set_nonblocking(int sock) {
  //   int opts;
  //   opts = fcntl(sock, F_GETFL);
  //   if (opts < 0) {
  //     ERR("set_nonblocking: get file status flags fail.");
  //   }
  //   opts = opts | O_NONBLOCK;
  //   if (fcntl(sock, F_SETFL, opts) < 0) {
  //     ERR("set_nonblocking: set file status flags fail.");
  //   }
  // }

  void on_connect(uv_connect_t *req, int status) {
    auto cb_ptr = static_cast<CallbackType *>(req->data);
    DEFER(delete cb_ptr;)

    if (*cb_ptr) {
      (*cb_ptr)(status);
    }
  }

  void Socket::Connect(std::string ip, int16_t port, CallbackType cb) {
    // sockaddr_in serv_addr;
    // serv_addr.sin_family = AF_INET;
    // serv_addr.sin_port = htons(port);

    // set_nonblocking(fd_);

    // // Convert IPv4 and IPv6 addresses from text to binary form
    // if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
    //   ERR("Invalid address:" << LOG_VALUE(ip));
    // }

    // if (connect(fd_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0
    //     && errno != EINPROGRESS) {
    //   ERR("Connect fail.");
    // }
    auto connect_req = new uv_connect_t();
    struct sockaddr_in dest;
    uv_ip4_addr(ip.c_str(), port, &dest);

    auto cb_p = new CallbackType(cb);
    connect_req->data = cb_p;

    uv_tcp_connect(connect_req, socket_.get(), (const struct sockaddr *)&dest, on_connect);
    uv_read_start(reinterpret_cast<uv_stream_t *>(socket_.get()), alloc_buffer, on_read_cb);
  }

  void after_write(uv_write_t *uv_req, int status) {
    auto cb_ptr = static_cast<CallbackType *>(uv_req->data);
    auto req = reinterpret_cast<write_req_t *>(uv_req);
    DEFER(delete cb_ptr; delete req;)

    if (*cb_ptr) {
      (*cb_ptr)(status);
    }
  }

  void Socket::Write(const char *data, int len, CallbackType cb) {
    // auto ret = write(fd_, data, len);
    // if (ret == -1) {
    //   ERR("Write err.");
    // }
    // return ret;
    auto req = new write_req_t();

    req->req.data = new CallbackType(cb);
    req->buf = uv_buf_init(const_cast<char *>(data), len);

    uv_write(reinterpret_cast<uv_write_t *>(req), reinterpret_cast<uv_stream_t *>(socket_.get()),
             &req->buf, 1, after_write);
  }

  void Socket::ReadStart() {}

  void Socket::Close() { uv_close(reinterpret_cast<uv_handle_t *>(socket_.get()), nullptr); }

  std::string HttpRequest::ToString() const {
    std::ostringstream oss;
    oss << method << " " << request_uri << " "
        << "HTTP/1.1"
        << "\r\n";
    for (auto iter = headers.begin(); iter != headers.end(); iter++) {
      oss << (**iter).name << ": " << (**iter).value << "\r\n";
    }
    oss << "\r\n";
    if (!body.empty()) oss << body << "\r\n";
    return oss.str();
  }

  std::string HttpResponse::ToString() const {
    ERR("do not implement.");  // TODO
  }

  void HttpReponseParser::Init() {
    parser_ = std::make_shared<http_parser>();
    http_parser_init(parser_.get(), HTTP_RESPONSE);
    parser_->data = this;

    http_parser_settings_init(&settings_);
    settings_.on_message_begin = OnMessageBegin;
    settings_.on_status = OnStatus;
    settings_.on_header_field = OnHeaderField;
    settings_.on_header_value = OnHeaderValue;
    settings_.on_body = OnBody;
    settings_.on_message_complete = OnMessageComplete;
  }

  int OnMessageBegin(http_parser *p) {
    auto parser = reinterpret_cast<HttpReponseParser *>(p->data);
    parser->res_ = std::make_shared<HttpResponse>();
    return 0;
  }
  int OnStatus(http_parser *p, const char *at, size_t length) {
    auto parser = reinterpret_cast<HttpReponseParser *>(p->data);
    parser->res_->status_code = std::string(at, length);
    return 0;
  }
  int OnHeaderField(http_parser *p, const char *at, size_t length) {
    auto parser = reinterpret_cast<HttpReponseParser *>(p->data);
    auto new_header = std::make_shared<HttpPacket::Header>();
    new_header->name = std::string(at, length);
    parser->res_->headers.push_back(new_header);
    return 0;
  }
  int OnHeaderValue(http_parser *p, const char *at, size_t length) {
    auto parser = reinterpret_cast<HttpReponseParser *>(p->data);
    auto header = parser->res_->headers.back();
    header->value = std::string(at, length);
    return 0;
  }
  int OnBody(http_parser *p, const char *at, size_t length) {
    auto parser = reinterpret_cast<HttpReponseParser *>(p->data);
    parser->res_->body = std::string(at, length);
    return 0;
  }
  int OnMessageComplete(http_parser *p) {
    auto parser = reinterpret_cast<HttpReponseParser *>(p->data);
    try {
      parser->on_complete(parser->res_);
    } catch (std::string e) {
      DLOG("parser->on_complete throw error: " << e);
    }
    return 0;
  }

  void HttpReponseParser::Feed(const char *data, size_t len) {
    size_t nparsed = http_parser_execute(parser_.get(), &settings_, data, len);
    if (nparsed != len) ERR("http_parser_execute error." << LOG_VALUE(parser_->http_errno));
  }
}  // namespace smark::util