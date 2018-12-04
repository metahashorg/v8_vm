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

vv::Error MapAcceptError(int os_error) {
  switch (os_error) {
    // If the client aborts the connection before the server calls accept,
    // POSIX specifies accept should fail with ECONNABORTED. The server can
    // ignore the error and just call accept again, so we map the error to
    // ERR_IO_PENDING. See UNIX Network Programming, Vol. 1, 3rd Ed., Sec.
    // 5.11, "Connection Abort before accept Returns".
    case ECONNABORTED:
      return vv::errNetIOPending ;
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

int SocketPosix::Open(int address_family) {
  // TODO:
  // DCHECK_EQ(kInvalidSocket, socket_fd_);
  // DCHECK(address_family == AF_INET ||
  //        address_family == AF_INET6 ||
  //        address_family == AF_UNIX);

  socket_fd_ = CreatePlatformSocket(
      address_family, SOCK_STREAM,
      address_family == AF_UNIX ? 0 : IPPROTO_TCP) ;
  if (socket_fd_ < 0) {
    printf("ERROR: CreatePlatformSocket() returned an error\n") ;
    return MapSystemError(errno) ;
  }

  if (!SetNonBlocking(socket_fd_)) {
    vv::Error rv = MapSystemError(errno) ;
    printf("ERROR: SetNonBlocking() returned a failure\n") ;
    Close() ;
    return rv ;
  }

  return vv::errOk ;
}

vv::Error SocketPosix::AdoptConnectedSocket(
    SocketDescriptor socket, const SockaddrStorage& address) {
  vv::Error rv = AdoptUnconnectedSocket(socket) ;
  if (rv != vv::errOk) {
    printf("ERROR: AdoptUnconnectedSocket() returned an error\n") ;
    return rv ;
  }

  SetPeerAddress(address) ;
  return vv::errOk ;
}

vv::Error SocketPosix::AdoptUnconnectedSocket(SocketDescriptor socket) {
  // TODO: DCHECK_EQ(kInvalidSocket, socket_fd_);

  socket_fd_ = socket ;

  if (!SetNonBlocking(socket_fd_)) {
    vv::Error rv = MapSystemError(errno) ;
    printf("ERROR: SetNonBlocking() returned a failure\n") ;
    Close() ;
    return rv ;
  }

  return vv::errOk ;
}

vv::Error SocketPosix::Bind(const SockaddrStorage& address) {
  // TODO: DCHECK_NE(kInvalidSocket, socket_fd_) ;

  int rv = bind(socket_fd_, address.addr, address.addr_len) ;
  if (rv < 0) {
    printf("ERROR: bind() returned an error\n") ;
    return MapSystemError(errno) ;
  }

  return vv::errOk ;
}

vv::Error SocketPosix::Listen(int backlog) {
  // TODO:
  // DCHECK_NE(kInvalidSocket, socket_fd_);
  // DCHECK_LT(0, backlog);

  int rv = listen(socket_fd_, backlog) ;
  if (rv < 0) {
    printf("ERROR: listen() returned an error\n") ;
    return MapSystemError(errno) ;
  }

  return vv::errOk ;
}

vv::Error SocketPosix::Accept(
    std::unique_ptr<SocketPosix>* socket, Timeout timeout) {
  // TODO:
  // DCHECK_NE(kInvalidSocket, socket_fd_);
  // DCHECK(socket);

  fd_set set ;
  FD_ZERO(&set) ;
  FD_SET(socket_fd_, &set) ;
  struct timeval timeout_val ;
  timeout_val.tv_sec  = timeout / 1000 ;
  timeout_val.tv_usec = (timeout % 1000) * 1000 ;
  int wait_result = HANDLE_EINTR(select(
      socket_fd_ + 1, &set, NULL, NULL, &timeout_val)) ;
  if(wait_result < 0) {
    printf("ERROR: select() returned an error\n") ;
    return MapAcceptError(errno) ;
  } else if (wait_result == 0) {
    return vv::errTimeout ;
  }

  SockaddrStorage new_peer_address ;
  int new_socket = HANDLE_EINTR(accept(
      socket_fd_, new_peer_address.addr, &new_peer_address.addr_len)) ;
  if (new_socket < 0) {
    printf("ERROR: accept() returned an error\n") ;
    return MapAcceptError(errno) ;
  }

  std::unique_ptr<SocketPosix> accepted_socket(new SocketPosix) ;
  vv::Error rv =
      accepted_socket->AdoptConnectedSocket(new_socket, new_peer_address) ;
  if (rv != vv::errOk) {
    return rv ;
  }

  *socket = std::move(accepted_socket) ;
  return vv::errOk ;
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

vv::Error SocketPosix::GetLocalAddress(SockaddrStorage* address) const {
  // TODO: DCHECK(address) ;

  if (getsockname(socket_fd_, address->addr, &address->addr_len) < 0) {
    printf("ERROR: getsockname() returned an error\n") ;
    return MapSystemError(errno) ;
  }

  return vv::errOk ;
}

vv::Error SocketPosix::GetPeerAddress(SockaddrStorage* address) const {
  // TODO: DCHECK(address);

  if (!HasPeerAddress()) {
    return vv::errNetSocketNotConnected ;
  }

  *address = *peer_address_ ;
  return vv::errOk ;
}

void SocketPosix::SetPeerAddress(const SockaddrStorage& address) {
  // |peer_address_| will be non-NULL if Connect() has been called. Unless
  // Close() is called to reset the internal state, a second call to Connect()
  // is not allowed.
  // Please note that we don't allow a second Connect() even if the previous
  // Connect() has failed. Connecting the same |socket_| again after a
  // connection attempt failed results in unspecified behavior according to
  // POSIX.
  // TODO: DCHECK(!peer_address_) ;
  peer_address_.reset(new SockaddrStorage(address)) ;
}

bool SocketPosix::HasPeerAddress() const {
  return peer_address_ != NULL ;
}

void SocketPosix::Close() {
  if (socket_fd_ != kInvalidSocket) {
    if (IGNORE_EINTR(close(socket_fd_)) < 0) {
      printf("ERROR: close() returned an error (%d)\n", errno) ;
    }

    socket_fd_ = kInvalidSocket ;
  }
}
