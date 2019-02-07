// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_apps/http_server/tcp-socket-win.h"

#include <errno.h>
#include <mstcpip.h>

#include "vm_apps/http_server/net-errors.h"
#include "vm_apps/http_server/sockaddr-storage.h"
#include "vm_apps/http_server/socket-options.h"
#include "vm_apps/http_server/winsock-init.h"

namespace {

const int kTCPKeepAliveSeconds = 45 ;

// Disable Nagle.
// Enable TCP Keep-Alive to prevent NAT routers from timing out TCP
// connections. See http://crbug.com/27400 for details.
bool SetTCPKeepAlive(SOCKET socket, BOOL enable, int delay_secs) {
  unsigned delay = delay_secs * 1000 ;
  struct tcp_keepalive keepalive_vals = {
      enable ? 1u : 0u,  // TCP keep-alive on.
      delay,  // Delay seconds before sending first TCP keep-alive packet.
      delay,  // Delay seconds between sending TCP keep-alive packets.
  } ;
  DWORD bytes_returned = 0xABAB ;
  int rv = WSAIoctl(socket, SIO_KEEPALIVE_VALS, &keepalive_vals,
                    sizeof(keepalive_vals), NULL, 0,
                    &bytes_returned, NULL, NULL) ;
  int os_error = WSAGetLastError() ;
  if (!!rv) {
    V8_LOG_ERR(
        MapSystemError(os_error),
        "Could not enable TCP Keep-Alive for socket, "
        "the last system error is %d", os_error) ;
  }

  // Disregard any failure in disabling nagle or enabling TCP Keep-Alive.
  return rv == 0 ;
}

bool SetNonBlockingAndGetError(SOCKET fd, int* os_error) {
  bool ret = false ;
  unsigned long nonblocking = 1 ;
  if (ioctlsocket(fd, FIONBIO, &nonblocking) == 0) {
    ret = true ;
  }

  *os_error = WSAGetLastError() ;
  return ret ;
}

}  // namespace

TcpSocketWin::TcpSocketWin()
  : socket_(INVALID_SOCKET),
    accept_event_(WSA_INVALID_EVENT),
    read_event_(WSA_INVALID_EVENT),
    write_event_(WSA_INVALID_EVENT),
    waiting_connect_(false),
    waiting_read_(false) {
  EnsureWinsockInit() ;
}

TcpSocketWin::~TcpSocketWin() {
  Close() ;
}

Error TcpSocketWin::Open(AddressFamily family) {
  // TODO: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) ;
  // TODO: DCHECK_EQ(socket_, INVALID_SOCKET) ;

  socket_ = CreatePlatformSocket(
      ConvertAddressFamily(family), SOCK_STREAM, IPPROTO_TCP) ;
  int os_error = WSAGetLastError() ;
  if (socket_ == INVALID_SOCKET) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "CreatePlatformSocket() is failed, the last system error is %d",
        os_error) ;
  }

  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    Error result = V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "SetNonBlocking() is failed, the last system error is %d", os_error) ;
    Close() ;
    return result ;
  }

  return errOk ;
}

Error TcpSocketWin::AdoptConnectedSocket(
    SocketDescriptor socket, const IPEndPoint& peer_address) {
  // TODO: DCHECK_EQ(socket_, INVALID_SOCKET);

  socket_ = socket ;

  int os_error ;
  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    Error result = V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "SetNonBlocking() is failed, the last system error is %d", os_error) ;
    Close() ;
    return result ;
  }

  peer_address_.reset(new IPEndPoint(peer_address)) ;
  return errOk ;
}

Error TcpSocketWin::AdoptUnconnectedSocket(SocketDescriptor socket) {
  // TODO: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) ;
  // TODO: DCHECK_EQ(socket_, INVALID_SOCKET) ;

  socket_ = socket ;

  int os_error ;
  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    Error result = V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "SetNonBlocking() is failed, the last system error is %d", os_error) ;
    Close() ;
    return result ;
  }

  // |core_| is not needed for sockets that are used to accept connections.
  // The operation here is more like Open but with an existing socket.

  return errOk ;
}

Error TcpSocketWin::Bind(const IPEndPoint& address) {
  // TODO: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO: DCHECK_NE(socket_, INVALID_SOCKET);

  SockaddrStorage storage ;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  int result = bind(socket_, storage.addr, storage.addr_len) ;
  int os_error = WSAGetLastError() ;
  if (result < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "bind() is failed, the last system error is %d", os_error) ;
  }

  return errOk ;
}

Error TcpSocketWin::Listen(int backlog) {
  // TODO: DCHECK_GT(backlog, 0) ;
  // TODO: DCHECK_NE(socket_, INVALID_SOCKET) ;
  // TODO: DCHECK_EQ(accept_event_, WSA_INVALID_EVENT) ;

  accept_event_ = WSACreateEvent() ;
  int os_error = WSAGetLastError() ;
  if (accept_event_ == WSA_INVALID_EVENT) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "WSACreateEvent() is failed, the last system error is %d", os_error) ;
  }

  int result = listen(socket_, backlog) ;
  os_error = WSAGetLastError() ;
  if (result < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "listen() is failed, the last system error is %d", os_error) ;
  }

  return errOk ;
}

Error TcpSocketWin::Accept(
    std::unique_ptr<TcpSocketWin>* socket, IPEndPoint* address,
    Timeout timeout) {
  // TODO: DCHECK(socket) ;
  // TODO: DCHECK(address) ;
  // TODO: DCHECK_NE(socket_, INVALID_SOCKET) ;
  // TODO: DCHECK_NE(accept_event_, WSA_INVALID_EVENT) ;

  WSAResetEvent(accept_event_) ;
  WSAEventSelect(socket_, accept_event_, FD_ACCEPT) ;
  int os_error = WSAGetLastError() ;
  int result = WSAWaitForMultipleEvents(
      1, &accept_event_, FALSE, timeout, FALSE) ;
  os_error = WSAGetLastError() ;
  if (result != WSA_WAIT_EVENT_0) {
    Error net_error = errTimeout ;
    if (result != WSA_WAIT_TIMEOUT) {
      net_error = V8_ERROR_CREATE_WITH_MSG_SP(
          MapSystemError(os_error),
          "WSAWaitForMultipleEvents() is failed, the last system error is %d",
          os_error) ;
    }

    return net_error ;
  }

  SockaddrStorage storage ;
  SOCKET new_socket = accept(socket_, storage.addr, &storage.addr_len) ;
  os_error = WSAGetLastError() ;
  if (new_socket < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "accept() is failed, the last system error is %d", os_error) ;
  }

  IPEndPoint ip_end_point ;
  if (!ip_end_point.FromSockAddr(storage.addr, storage.addr_len)) {
    // TODO: NOTREACHED();
    if (closesocket(new_socket) < 0) {
      os_error = WSAGetLastError() ;
      V8_LOG_ERR(
          MapSystemError(os_error),
          "closesocket() is failed, the last system error is %d", os_error) ;
    }

    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  std::unique_ptr<TcpSocketWin> tcp_socket(new TcpSocketWin()) ;
  Error adopt_result =
      tcp_socket->AdoptConnectedSocket(new_socket, ip_end_point) ;
  V8_ERROR_RETURN_IF_FAILED(adopt_result) ;

  *socket = std::move(tcp_socket) ;
  *address = ip_end_point ;
  return errOk ;
}

bool TcpSocketWin::IsConnected() const {
  if (socket_ == INVALID_SOCKET || waiting_connect_) {
    return false ;
  }

  if (waiting_read_) {
    return true ;
  }

  // Check if connection is alive.
  char c ;
  int rv = recv(socket_, &c, 1, MSG_PEEK) ;
  int os_error = WSAGetLastError() ;
  if (rv == 0) {
    return false ;
  }

  if (rv == SOCKET_ERROR && os_error != WSAEWOULDBLOCK) {
    return false ;
  }

  return true ;
}

bool TcpSocketWin::IsConnectedAndIdle() const {
  if (socket_ == INVALID_SOCKET || waiting_connect_) {
    return false ;
  }

  if (waiting_read_) {
    return true ;
  }

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c ;
  int rv = recv(socket_, &c, 1, MSG_PEEK) ;
  int os_error = WSAGetLastError() ;
  if (rv >= 0) {
    return false ;
  }

  if (os_error != WSAEWOULDBLOCK) {
    return false ;
  }

  return true ;
}

Error TcpSocketWin::Read(char* buf, std::int32_t& buf_len, Timeout timeout) {
  // TODO: DCHECK_NE(socket_, INVALID_SOCKET) ;
  // TODO: DCHECK(!waiting_read_) ;

  // Set a flag of waiting read
  TemporarilySetValue<bool> waiting_read(waiting_read_, true) ;

  // Remember a buffer length and set a result of reading to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Create a read event the first time
  if (read_event_ == WSA_INVALID_EVENT) {
    read_event_ = WSACreateEvent() ;
    int os_error = WSAGetLastError() ;
    if (read_event_ == WSA_INVALID_EVENT) {
      return V8_ERROR_CREATE_WITH_MSG_SP(
          MapSystemError(os_error),
          "WSACreateEvent() is failed, the last system error is %d", os_error) ;
    }
  }

  // Wait when we'll have something for reading
  WSAResetEvent(read_event_) ;
  WSAEventSelect(socket_, read_event_, FD_READ | FD_CLOSE) ;
  int os_error = WSAGetLastError() ;
  int result = WSAWaitForMultipleEvents(
      1, &read_event_, FALSE, timeout, FALSE) ;
  os_error = WSAGetLastError() ;
  if (result != WSA_WAIT_EVENT_0) {
    Error net_error = errTimeout ;
    if (result != WSA_WAIT_TIMEOUT) {
      net_error = V8_ERROR_CREATE_WITH_MSG_SP(
          MapSystemError(os_error),
          "WSAWaitForMultipleEvents() is failed, the last system error is %d",
          os_error) ;
    }

    return net_error ;
  }

  // Try to read
  result = recv(socket_, buf, local_buf_len, 0) ;
  os_error = WSAGetLastError() ;
  if (result == SOCKET_ERROR) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "recv() is failed, the last system error is %d", os_error) ;
  }

  buf_len = result ;
  return errOk ;
}

Error TcpSocketWin::Write(
    const char* buf, std::int32_t& buf_len, Timeout timeout) {
  // TODO: DCHECK_NE(socket_, INVALID_SOCKET) ;
  // TODO: DCHECK(!waiting_write_) ;

  // Set a flag of waiting read
  TemporarilySetValue<bool> waiting_wtite(waiting_write_, true) ;

  // Remember a buffer length and set a result of writing to 0
  std::int32_t local_buf_len = buf_len ;
  buf_len = 0 ;

  // Create a read event the first time
  if (write_event_ == WSA_INVALID_EVENT) {
    write_event_ = WSACreateEvent() ;
    int os_error = WSAGetLastError() ;
    if (write_event_ == WSA_INVALID_EVENT) {
      return V8_ERROR_CREATE_WITH_MSG_SP(
          MapSystemError(os_error),
          "WSACreateEvent() is failed, the last system error is %d", os_error) ;
    }
  }

  // Wait when we'll have something for reading
  WSAResetEvent(write_event_) ;
  WSAEventSelect(socket_, write_event_, FD_WRITE | FD_CLOSE) ;
  int os_error = WSAGetLastError() ;
  int result = WSAWaitForMultipleEvents(
      1, &write_event_, FALSE, timeout, FALSE) ;
  os_error = WSAGetLastError() ;
  if (result != WSA_WAIT_EVENT_0) {
    Error net_error = errTimeout ;
    if (result != WSA_WAIT_TIMEOUT) {
      net_error = V8_ERROR_CREATE_WITH_MSG_SP(
          MapSystemError(os_error),
          "WSAWaitForMultipleEvents() is failed, the last system error is %d",
          os_error) ;
    }

    return net_error ;
  }

  // Try to read
  WSABUF write_buffer ;
  write_buffer.len = local_buf_len ;
  write_buffer.buf = const_cast<char*>(buf) ;
  DWORD num = 0 ;
  result = WSASend(socket_, &write_buffer, 1, &num, 0, NULL, NULL) ;
  os_error = WSAGetLastError() ;
  if (result == SOCKET_ERROR) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "WSASend() is failed, the last system error is %d", os_error) ;
  }

  buf_len = num ;
  return errOk ;
}

Error TcpSocketWin::GetLocalAddress(IPEndPoint* address) const {
  // TODO: DCHECK(address) ;

  SockaddrStorage storage ;
  if (getsockname(socket_, storage.addr, &storage.addr_len)) {
    int os_error = WSAGetLastError() ;
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(os_error),
        "getsockname() is failed, the last system error is %d", os_error) ;
  }
  if (!address->FromSockAddr(storage.addr, storage.addr_len)) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetAddressInvalid, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  return errOk ;
}

Error TcpSocketWin::GetPeerAddress(IPEndPoint* address) const {
  // TODO: DCHECK(address) ;
  if (!IsConnected()) {
    return V8_ERROR_CREATE_WITH_MSG(
        errNetSocketNotConnected, V8_ERROR_MSG_FUNCTION_FAILED()) ;
  }

  *address = *peer_address_ ;
  return errOk ;
}

Error TcpSocketWin::SetDefaultOptionsForServer() {
  return SetExclusiveAddrUse() ;
}

void TcpSocketWin::SetDefaultOptionsForClient() {
  SetTCPNoDelay(socket_, /*no_delay=*/true) ;
  SetTCPKeepAlive(socket_, true, kTCPKeepAliveSeconds) ;
}

Error TcpSocketWin::SetExclusiveAddrUse() {
  // On Windows, a bound end point can be hijacked by another process by
  // setting SO_REUSEADDR. Therefore a Windows-only option SO_EXCLUSIVEADDRUSE
  // was introduced in Windows NT 4.0 SP4. If the socket that is bound to the
  // end point has SO_EXCLUSIVEADDRUSE enabled, it is not possible for another
  // socket to forcibly bind to the end point until the end point is unbound.
  // It is recommend that all server applications must use SO_EXCLUSIVEADDRUSE.
  // MSDN: http://goo.gl/M6fjQ.
  //
  // Unlike on *nix, on Windows a TCP server socket can always bind to an end
  // point in TIME_WAIT state without setting SO_REUSEADDR, therefore it is not
  // needed here.
  //
  // SO_EXCLUSIVEADDRUSE will prevent a TCP client socket from binding to an end
  // point in TIME_WAIT status. It does not have this effect for a TCP server
  // socket.

  BOOL true_value = 1 ;
  int rv = setsockopt(socket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                      reinterpret_cast<const char*>(&true_value),
                      sizeof(true_value)) ;
  if (rv < 0) {
    return V8_ERROR_CREATE_WITH_MSG_SP(
        MapSystemError(errno),
        "setsockopt() is failed, the last system error is %d", errno) ;
  }

  return errOk ;
}

Error TcpSocketWin::SetReceiveBufferSize(std::int32_t size) {
  return SetSocketReceiveBufferSize(socket_, size) ;
}

Error TcpSocketWin::SetSendBufferSize(std::int32_t size) {
  return SetSocketSendBufferSize(socket_, size) ;
}

bool TcpSocketWin::SetKeepAlive(bool enable, int delay) {
  return SetTCPKeepAlive(socket_, enable, delay) ;
}

bool TcpSocketWin::SetNoDelay(bool no_delay) {
  return SetTCPNoDelay(socket_, no_delay) == errOk ;
}

void TcpSocketWin::Close() {
  // TODO: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) ;

  if (socket_ != INVALID_SOCKET) {
    // Note: don't use CancelIo to cancel pending IO because it doesn't work
    // when there is a Winsock layered service provider.

    // In most socket implementations, closing a socket results in a graceful
    // connection shutdown, but in Winsock we have to call shutdown explicitly.
    // See the MSDN page "Graceful Shutdown, Linger Options, and Socket Closure"
    // at http://msdn.microsoft.com/en-us/library/ms738547.aspx
    shutdown(socket_, SD_SEND) ;

    // This cancels any pending IO.
    if (closesocket(socket_) < 0) {
      int os_error = WSAGetLastError() ;
      V8_LOG_ERR(
          MapSystemError(os_error),
          "closesocket() is failed, the last system error is %d", os_error) ;
    }

    socket_ = INVALID_SOCKET ;
  }

  if (accept_event_ != WSA_INVALID_EVENT) {
    WSACloseEvent(accept_event_) ;
    accept_event_ = WSA_INVALID_EVENT ;
  }

  if (read_event_ != WSA_INVALID_EVENT) {
    WSACloseEvent(read_event_) ;
    read_event_ = WSA_INVALID_EVENT ;
  }

  if (write_event_ != WSA_INVALID_EVENT) {
    WSACloseEvent(write_event_) ;
    write_event_ = WSA_INVALID_EVENT ;
  }

  peer_address_.reset() ;
}
