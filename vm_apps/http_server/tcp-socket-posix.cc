// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/tcp-socket-posix.h"

#include <errno.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "vm_apps/http_server/net-errors.h"
#include "vm_apps/http_server/sockaddr-storage.h"
#include "vm_apps/http_server/socket-options.h"

namespace {

// SetTCPKeepAlive sets SO_KEEPALIVE.
bool SetTCPKeepAlive(int fd, bool enable, int delay) {
  // Enabling TCP keepalives is the same on all platforms.
  int on = enable ? 1 : 0 ;
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on))) {
    V8_LOG_ERR(
        MapSystemError(errno),
        "Failed to set SO_KEEPALIVE on fd, the last system error is %d",
        errno) ;
    return false ;
  }

  // If we disabled TCP keep alive, our work is done here.
  if (!enable) {
    return true ;
  }

#if defined(V8_OS_LINUX) || defined(V8_OS_ANDROID)
  // Setting the keepalive interval varies by platform.

  // Set seconds until first TCP keep alive.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &delay, sizeof(delay))) {
    V8_LOG_ERR(
        MapSystemError(errno),
        "Failed to set TCP_KEEPIDLE on fd, the last system error is %d",
        errno) ;
    return false ;
  }
  // Set seconds between TCP keep alives.
  if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &delay, sizeof(delay))) {
    V8_LOG_ERR(
        MapSystemError(errno),
        "Failed to set TCP_KEEPINTVL on fd, the last system error is %d",
        errno) ;
    return false ;
  }
#elif defined(V8_OS_MACOSX) || defined(V8_OS_IOS)
  if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &delay, sizeof(delay))) {
    V8_LOG_ERR(
        MapSystemError(errno),
        "Failed to set TCP_KEEPALIVE on fd, the last system error is %d",
        errno) ;
    return false ;
  }
#endif
  return true ;
}

}  // namespace

TcpSocketPosix::TcpSocketPosix() {}

TcpSocketPosix::~TcpSocketPosix() {
  Close() ;
}

Error TcpSocketPosix::Open(AddressFamily family) {
  // TODO: DCHECK(!socket_) ;
  socket_.reset(new SocketPosix) ;
  Error rv = socket_->Open(ConvertAddressFamily(family)) ;
  if (rv != errOk) {
    V8_ERROR_ADD_MSG(rv, V8_ERROR_MSG_FUNCTION_FAILED()) ;
    socket_.reset() ;
  }

  return rv ;
}

Error TcpSocketPosix::AdoptConnectedSocket(
    SocketDescriptor socket, const IPEndPoint& peer_address) {
  // TODO: DCHECK(!socket_) ;

  SockaddrStorage storage ;
  if (!peer_address.ToSockAddr(storage.addr, &storage.addr_len) &&
      // For backward compatibility, allows the empty address.
      !(peer_address == IPEndPoint())) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  socket_.reset(new SocketPosix) ;
  Error rv = socket_->AdoptConnectedSocket(socket, storage) ;
  if (rv != errOk) {
    V8_ERROR_ADD_MSG(rv, V8_ERROR_MSG_FUNCTION_FAILED()) ;
    socket_.reset() ;
  }

  return rv ;
}

Error TcpSocketPosix::AdoptUnconnectedSocket(SocketDescriptor socket) {
  // TODO: DCHECK(!socket_);

  socket_.reset(new SocketPosix) ;
  Error rv = socket_->AdoptUnconnectedSocket(socket) ;
  if (rv != errOk) {
    V8_ERROR_ADD_MSG(rv, V8_ERROR_MSG_FUNCTION_FAILED()) ;
    socket_.reset() ;
  }

  return rv ;
}

Error TcpSocketPosix::Bind(const IPEndPoint& address) {
  // TODO: DCHECK(socket_) ;

  SockaddrStorage storage ;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return socket_->Bind(storage) ;
}

Error TcpSocketPosix::Listen(int backlog) {
  // TODO: DCHECK(socket_) ;
  return socket_->Listen(backlog) ;
}

Error TcpSocketPosix::Accept(
    std::unique_ptr<TcpSocketPosix>* tcp_socket, IPEndPoint* address,
    Timeout timeout) {
  // TODO: DCHECK(tcp_socket);
  // TODO: DCHECK(socket_);

  std::unique_ptr<SocketPosix> accept_socket ;
  Error rv = socket_->Accept(&accept_socket, timeout) ;
  if (V8_ERROR_FAILED(rv)) {
    if (rv != errTimeout) {
      V8_ERROR_ADD_MSG(rv, V8_ERROR_MSG_FUNCTION_FAILED()) ;
    }

    return rv ;
  }

  SockaddrStorage storage ;
  if (accept_socket->GetPeerAddress(&storage) != errOk ||
      !address->FromSockAddr(storage.addr, storage.addr_len)) {
    accept_socket.reset() ;
    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  tcp_socket->reset(new TcpSocketPosix()) ;
  (*tcp_socket)->socket_ = std::move(accept_socket) ;
  return errOk ;
}

bool TcpSocketPosix::IsConnected() const {
  if (!socket_) {
    return false ;
  }

  return socket_->IsConnected();
}

bool TcpSocketPosix::IsConnectedAndIdle() const {
  // TODO(wtc): should we also handle the TCP FastOpen case here,
  // as we do in IsConnected()?
  return socket_ && socket_->IsConnectedAndIdle() ;
}

Error TcpSocketPosix::Read(
    char* buf, std::int32_t& buf_len, Timeout timeout) {
  // TODO: DCHECK(socket_) ;

  return socket_->Read(buf, buf_len, timeout) ;
}

Error TcpSocketPosix::Write(
    const char* buf, std::int32_t& buf_len, Timeout timeout) {
  // TODO: DCHECK(socket_) ;

  return socket_->Write(buf, buf_len, timeout) ;
}

Error TcpSocketPosix::GetLocalAddress(IPEndPoint* address) const {
  // TODO: DCHECK(address) ;

  if (!socket_) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetSocketNotConnected, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  SockaddrStorage storage ;
  Error rv = socket_->GetLocalAddress(&storage) ;
  V8_ERROR_RETURN_IF_FAILED(rv) ;
  if (!address->FromSockAddr(storage.addr, storage.addr_len)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return errOk ;
}

Error TcpSocketPosix::GetPeerAddress(IPEndPoint* address) const {
  // TODO: DCHECK(address);

  if (!IsConnected()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetSocketNotConnected, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  SockaddrStorage storage ;
  Error rv = socket_->GetPeerAddress(&storage) ;
  V8_ERROR_RETURN_IF_FAILED(rv) ;

  if (!address->FromSockAddr(storage.addr, storage.addr_len)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return errOk ;
}

Error TcpSocketPosix::SetDefaultOptionsForServer() {
  // TODO: DCHECK(socket_);
  return AllowAddressReuse() ;
}

void TcpSocketPosix::SetDefaultOptionsForClient() {
  // TODO: DCHECK(socket_);

  // This mirrors the behaviour on Windows. See the comment in
  // tcp_socket_win.cc after searching for "NODELAY".
  // If SetTCPNoDelay fails, we don't care.
  SetTCPNoDelay(socket_->socket_fd(), true) ;

#if !defined(V8_OS_ANDROID) && !defined(V8_OS_IOS) && !defined(V8_OS_FUCHSIA)
  // TCP keep alive wakes up the radio, which is expensive on mobile.
  // It's also not implemented on Fuchsia. Do not enable it there.
  // TODO(crbug.com/758706): Consider enabling keep-alive on Fuchsia.
  //
  // It's useful to prevent TCP middleboxes from timing out
  // connection mappings. Packets for timed out connection mappings at
  // middleboxes will either lead to:
  // a) Middleboxes sending TCP RSTs. It's up to higher layers to check for this
  // and retry. The HTTP network transaction code does this.
  // b) Middleboxes just drop the unrecognized TCP packet. This leads to the TCP
  // stack retransmitting packets per TCP stack retransmission timeouts, which
  // are very high (on the order of seconds). Given the number of
  // retransmissions required before killing the connection, this can lead to
  // tens of seconds or even minutes of delay, depending on OS.
  const int kTCPKeepAliveSeconds = 45 ;

  SetTCPKeepAlive(socket_->socket_fd(), true, kTCPKeepAliveSeconds) ;
#endif
}

Error TcpSocketPosix::AllowAddressReuse() {
  // TODO: DCHECK(socket_) ;

  return SetReuseAddr(socket_->socket_fd(), true) ;
}

Error TcpSocketPosix::SetReceiveBufferSize(std::int32_t size) {
  // TODO: DCHECK(socket_) ;

  return SetSocketReceiveBufferSize(socket_->socket_fd(), size) ;
}

Error TcpSocketPosix::SetSendBufferSize(std::int32_t size) {
  // TODO: DCHECK(socket_) ;

  return SetSocketSendBufferSize(socket_->socket_fd(), size) ;
}

bool TcpSocketPosix::SetKeepAlive(bool enable, int delay) {
  // TODO: DCHECK(socket_) ;

  return SetTCPKeepAlive(socket_->socket_fd(), enable, delay) ;
}

bool TcpSocketPosix::SetNoDelay(bool no_delay) {
  // TODO: DCHECK(socket_) ;

  return SetTCPNoDelay(socket_->socket_fd(), no_delay) == errOk ;
}

void TcpSocketPosix::Close() {
  socket_.reset() ;
}

bool TcpSocketPosix::IsValid() const {
  return socket_ != NULL && socket_->socket_fd() != kInvalidSocket ;
}
