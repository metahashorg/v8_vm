// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/socket-posix.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <utility>
#include <unistd.h>

#include "src/base/build_config.h"
#include "vm_apps/http_server/eintr-wrapper.h"
#include "vm_apps/http_server/ip-endpoint.h"
#include "vm_apps/http_server/net-errors.h"
#include "vm_apps/http_server/sockaddr-storage.h"

#if defined(V8_OS_FUCHSIA)
#include <poll.h>
#include <sys/ioctl.h>
#endif  // V8_OS_FUCHSIA

namespace {

Error MapAcceptError(int os_error) {
  switch (os_error) {
    // If the client aborts the connection before the server calls accept,
    // POSIX specifies accept should fail with ECONNABORTED. The server can
    // ignore the error and just call accept again, so we map the error to
    // ERR_IO_PENDING. See UNIX Network Programming, Vol. 1, 3rd Ed., Sec.
    // 5.11, "Connection Abort before accept Returns".
    case ECONNABORTED:
      return errNetIOPending ;
    default:
      return MapSystemError(os_error) ;
  }
}

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL) ;
  if (flags == -1) {
    return false ;
  }

  if (flags & O_NONBLOCK) {
    return true ;
  }

  if (HANDLE_EINTR(fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1) {
    return false ;
  }

  return true ;
}

}  //namespace

SocketPosix::SocketPosix()
  : socket_fd_(kInvalidSocket),
    waiting_connect_(false) {}

SocketPosix::~SocketPosix() {
  Close();
}

Error SocketPosix::Open(int address_family) {
  DCHECK_EQ(kInvalidSocket, socket_fd_) ;
  DCHECK(address_family == AF_INET ||
         address_family == AF_INET6 ||
         address_family == AF_UNIX) ;

  socket_fd_ = CreatePlatformSocket(
      address_family, SOCK_STREAM,
      address_family == AF_UNIX ? 0 : IPPROTO_TCP) ;
  if (socket_fd_ < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "CreatePlatformSocket() is failed, the last system error is %d",
        errno) ;
  }

  if (!SetNonBlocking(socket_fd_)) {
    Error rv = V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "SetNonBlocking() is failed, the last system error is %d", errno) ;
    Close() ;
    return rv ;
  }

  return errOk ;
}

Error SocketPosix::AdoptConnectedSocket(
    SocketDescriptor socket, const SockaddrStorage& address) {
  Error rv = AdoptUnconnectedSocket(socket) ;
  V8_ERROR_RETURN_IF_FAILED(rv) ;
  SetPeerAddress(address) ;
  return errOk ;
}

Error SocketPosix::AdoptUnconnectedSocket(SocketDescriptor socket) {
  DCHECK_EQ(kInvalidSocket, socket_fd_) ;

  socket_fd_ = socket ;

  if (!SetNonBlocking(socket_fd_)) {
    Error rv = V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "SetNonBlocking() is failed, the last system error is %d", errno) ;
    Close() ;
    return rv ;
  }

  return errOk ;
}

Error SocketPosix::Bind(const SockaddrStorage& address) {
  DCHECK_NE(kInvalidSocket, socket_fd_) ;

  int rv = bind(socket_fd_, address.addr, address.addr_len) ;
  if (rv < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno), "bind() is failed, the last system error is %d",
        errno) ;
  }

  return errOk ;
}

Error SocketPosix::Listen(int backlog) {
  DCHECK_NE(kInvalidSocket, socket_fd_) ;
  DCHECK_LT(0, backlog) ;

  int rv = listen(socket_fd_, backlog) ;
  if (rv < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "listen() is failed, the last system error is %d", errno) ;
  }

  return errOk ;
}

Error SocketPosix::Accept(
    std::unique_ptr<SocketPosix>* socket, Timeout timeout) {
  DCHECK_NE(kInvalidSocket, socket_fd_) ;
  DCHECK(socket) ;
  DCHECK(timeout >= 0 || timeout == kInfiniteTimeout) ;

  fd_set set ;
  FD_ZERO(&set) ;
  FD_SET(socket_fd_, &set) ;
  struct timeval timeout_val, *timeout_pval = nullptr ;
  if (timeout != kInfiniteTimeout) {
    timeout_val.tv_sec  = timeout / 1000 ;
    timeout_val.tv_usec = (timeout % 1000) * 1000 ;
    timeout_pval = &timeout_val ;
  }

  int wait_result = HANDLE_EINTR(select(
      socket_fd_ + 1, &set, NULL, NULL, timeout_pval)) ;
  if(wait_result < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapAcceptError(errno),
        "select() is failed, the last system error is %d", errno) ;
  } else if (wait_result == 0) {
    return errTimeout ;
  }

  SockaddrStorage new_peer_address ;
  int new_socket = HANDLE_EINTR(accept(
      socket_fd_, new_peer_address.addr, &new_peer_address.addr_len)) ;
  if (new_socket < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapAcceptError(errno),
        "accept() is failed, the last system error is %d", errno) ;
  }

  std::unique_ptr<SocketPosix> accepted_socket(new SocketPosix) ;
  Error rv =
      accepted_socket->AdoptConnectedSocket(new_socket, new_peer_address) ;
  if (rv != errOk) {
    return rv ;
  }

  *socket = std::move(accepted_socket) ;
  return errOk ;
}

bool SocketPosix::IsConnected() const {
  if (socket_fd_ == kInvalidSocket || waiting_connect_) {
    return false ;
  }

#if !defined(V8_OS_FUCHSIA)
  // Checks if connection is alive.
  char c ;
  long rv = HANDLE_EINTR(recv(socket_fd_, &c, 1, MSG_PEEK)) ;
  if (rv == 0) {
    return false ;
  }

  if (rv == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    return false ;
  }

  return true ;

#else   // V8_OS_FUCHSIA
  // Fuchsia currently doesn't support MSG_PEEK flag in recv(), so the code
  // above doesn't work on Fuchsia. IsConnected() must return true if the
  // connection is alive or if it was terminated but there is still data pending
  // in the incoming buffer.
  //   1. Check if the connection is alive using poll(POLLRDHUP).
  //   2. If closed, then use ioctl(FIONREAD) to check if there is data to be
  //   read.
  // TODO(bug 738275): Remove once MSG_PEEK is implemented on Fuchsia.
  struct pollfd pollfd ;
  pollfd.fd = socket_fd_ ;
  pollfd.events = POLLRDHUP ;
  pollfd.revents = 0 ;
  const int poll_result = HANDLE_EINTR(poll(&pollfd, 1, 0)) ;

  if (poll_result == 1) {
    int bytes_available ;
    int ioctl_result =
        HANDLE_EINTR(ioctl(socket_fd_, FIONREAD, &bytes_available)) ;
    return ioctl_result == 0 && bytes_available > 0 ;
  }

  return poll_result == 0 ;
#endif  // V8_OS_FUCHSIA
}

bool SocketPosix::IsConnectedAndIdle() const {
  if (socket_fd_ == kInvalidSocket || waiting_connect_) {
    return false ;
  }

#if !defined(V8_OS_FUCHSIA)
  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c ;
  long rv = HANDLE_EINTR(recv(socket_fd_, &c, 1, MSG_PEEK)) ;
  if (rv >= 0) {
    return false ;
  }

  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    return false ;
  }

  return true ;

#else   // V8_OS_FUCHSIA
  // Fuchsia currently doesn't support MSG_PEEK flag in recv(), so the code
  // above doesn't work on Fuchsia. Use poll(POLLIN) to check state of the
  // socket. POLLIN is signaled if the socket is readable or if it was closed by
  // the peer, i.e. the socket is connected and idle if and only if POLLIN is
  // not signaled.
  // TODO(bug 738275): Remove once MSG_PEEK is implemented.
  struct pollfd pollfd ;
  pollfd.fd = socket_fd_ ;
  pollfd.events = POLLIN ;
  pollfd.revents = 0 ;
  const int poll_result = HANDLE_EINTR(poll(&pollfd, 1, 0)) ;
  return poll_result == 0 ;
#endif  // V8_OS_FUCHSIA
}

Error SocketPosix::Read(char* buf, std::int32_t& buf_len, Timeout timeout) {
  DCHECK_NE(kInvalidSocket, socket_fd_) ;
  DCHECK(!waiting_connect_) ;
  DCHECK_NE(nullptr, buf) ;
  DCHECK_LT(0, buf_len) ;
  DCHECK(timeout >= 0 || timeout == kInfiniteTimeout) ;

  // Remember a buffer length and set a result of reading to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Wait when we'll have something for reading
  fd_set set ;
  FD_ZERO(&set) ;
  FD_SET(socket_fd_, &set) ;
  struct timeval timeout_val, *timeout_pval = nullptr ;
  if (timeout != kInfiniteTimeout) {
    timeout_val.tv_sec  = timeout / 1000 ;
    timeout_val.tv_usec = (timeout % 1000) * 1000 ;
    timeout_pval = &timeout_val ;
  }

  int wait_result = HANDLE_EINTR(select(
      socket_fd_ + 1, &set, NULL, NULL, timeout_pval)) ;
  if(wait_result < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapAcceptError(errno),
        "select() is failed, the last system error is %d", errno) ;
  } else if (wait_result == 0) {
    return errTimeout ;
  }

  // Try to read
  long rv = HANDLE_EINTR(read(socket_fd_, buf, local_buf_len)) ;
  if (rv < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "read() is failed, the last system error is %d", errno) ;
  }

  buf_len = static_cast<std::int32_t>(rv) ;
  return errOk ;
}

Error SocketPosix::Write(
    const char* buf, std::int32_t& buf_len, Timeout timeout) {
  DCHECK_NE(kInvalidSocket, socket_fd_) ;
  DCHECK(!waiting_connect_) ;
  DCHECK_NE(nullptr, buf) ;
  DCHECK_LT(0, buf_len) ;
  DCHECK(timeout >= 0 || timeout == kInfiniteTimeout) ;

  // Remember a buffer length and set a result of writing to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Wait when we'll be able to write
  fd_set set ;
  FD_ZERO(&set) ;
  FD_SET(socket_fd_, &set) ;
  struct timeval timeout_val, *timeout_pval = nullptr ;
  if (timeout != kInfiniteTimeout) {
    timeout_val.tv_sec  = timeout / 1000 ;
    timeout_val.tv_usec = (timeout % 1000) * 1000 ;
    timeout_pval = &timeout_val ;
  }

  int wait_result = HANDLE_EINTR(select(
      socket_fd_ + 1, NULL, &set, NULL, timeout_pval)) ;
  if(wait_result < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapAcceptError(errno),
        "select() is failed, the last system error is %d", errno) ;
  } else if (wait_result == 0) {
    return errTimeout ;
  }

#if defined(V8_OS_LINUX) || defined(V8_OS_ANDROID)
  // Disable SIGPIPE for this write. Although Chromium globally disables
  // SIGPIPE, the net stack may be used in other consumers which do not do
  // this. MSG_NOSIGNAL is a Linux-only API. On OS X, this is a setsockopt on
  // socket creation.
  long rv = HANDLE_EINTR(send(socket_fd_, buf, local_buf_len, MSG_NOSIGNAL)) ;
#else
  long rv = HANDLE_EINTR(write(socket_fd_, buf, local_buf_len)) ;
#endif

  if (rv < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "send() is failed, the last system error is %d", errno) ;
  }

  buf_len = static_cast<std::int32_t>(rv) ;
  return errOk ;
}

Error SocketPosix::GetLocalAddress(SockaddrStorage* address) const {
  DCHECK(address) ;

  if (getsockname(socket_fd_, address->addr, &address->addr_len) < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "getsockname() is failed, the last system error is %d", errno) ;
  }

  return errOk ;
}

Error SocketPosix::GetPeerAddress(SockaddrStorage* address) const {
  DCHECK(address) ;

  if (!HasPeerAddress()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetSocketNotConnected, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  *address = *peer_address_ ;
  return errOk ;
}

void SocketPosix::SetPeerAddress(const SockaddrStorage& address) {
  // |peer_address_| will be non-NULL if Connect() has been called. Unless
  // Close() is called to reset the internal state, a second call to Connect()
  // is not allowed.
  // Please note that we don't allow a second Connect() even if the previous
  // Connect() has failed. Connecting the same |socket_| again after a
  // connection attempt failed results in unspecified behavior according to
  // POSIX.
  DCHECK(!peer_address_) ;
  peer_address_.reset(new SockaddrStorage(address)) ;
}

bool SocketPosix::HasPeerAddress() const {
  return peer_address_ != NULL ;
}

void SocketPosix::Close() {
  if (socket_fd_ != kInvalidSocket) {
    if (IGNORE_EINTR(close(socket_fd_)) < 0) {
      V8_LOG_ERR(
          MapSystemError(errno),
          "close() is failed, the last system error is %d", errno) ;
    }

    socket_fd_ = kInvalidSocket ;
  }
}
