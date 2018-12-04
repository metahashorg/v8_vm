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
    printf("ERROR: Could not enable TCP Keep-Alive for socket (0x%08x)\n",
           os_error) ;
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
    waiting_connect_(false),
    waiting_read_(false) {
  EnsureWinsockInit() ;
}

TcpSocketWin::~TcpSocketWin() {
  Close() ;
}

vv::Error TcpSocketWin::Open(AddressFamily family) {
  // TODO: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) ;
  // TODO: DCHECK_EQ(socket_, INVALID_SOCKET) ;

  socket_ = CreatePlatformSocket(
      ConvertAddressFamily(family), SOCK_STREAM, IPPROTO_TCP) ;
  int os_error = WSAGetLastError() ;
  if (socket_ == INVALID_SOCKET) {
    printf("ERROR: CreatePlatformSocket() returned an error\n") ;
    return MapSystemError(os_error) ;
  }

  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    vv::Error result = MapSystemError(os_error) ;
    printf("ERROR: SetNonBlocking() returned an error\n") ;
    Close() ;
    return result ;
  }

  return vv::errOk ;
}

vv::Error TcpSocketWin::AdoptConnectedSocket(
    SocketDescriptor socket, const IPEndPoint& peer_address) {
  // TODO: DCHECK_EQ(socket_, INVALID_SOCKET);

  socket_ = socket ;

  int os_error ;
  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    int result = MapSystemError(os_error) ;
    printf("ERROR: SetNonBlocking() returned an error\n") ;
    Close() ;
    return result ;
  }

  peer_address_.reset(new IPEndPoint(peer_address)) ;
  return vv::errOk ;
}

vv::Error TcpSocketWin::AdoptUnconnectedSocket(SocketDescriptor socket) {
  // TODO: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) ;
  // TODO: DCHECK_EQ(socket_, INVALID_SOCKET) ;

  socket_ = socket ;

  int os_error ;
  if (!SetNonBlockingAndGetError(socket_, &os_error)) {
    int result = MapSystemError(os_error) ;
    printf("ERROR: SetNonBlocking() returned an error\n") ;
    Close() ;
    return result ;
  }

  // |core_| is not needed for sockets that are used to accept connections.
  // The operation here is more like Open but with an existing socket.

  return vv::errOk ;
}

vv::Error TcpSocketWin::Bind(const IPEndPoint& address) {
  // TODO: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO: DCHECK_NE(socket_, INVALID_SOCKET);

  SockaddrStorage storage ;
  if (!address.ToSockAddr(storage.addr, &storage.addr_len)) {
    return vv::errNetAddressInvalid ;
  }

  int result = bind(socket_, storage.addr, storage.addr_len) ;
  int os_error = WSAGetLastError() ;
  if (result < 0) {
    printf("ERROR: bind() returned an error\n") ;
    return MapSystemError(os_error) ;
  }

  return vv::errOk ;
}

vv::Error TcpSocketWin::Listen(int backlog) {
  // TODO: DCHECK_GT(backlog, 0) ;
  // TODO: DCHECK_NE(socket_, INVALID_SOCKET) ;
  // TODO: DCHECK_EQ(accept_event_, WSA_INVALID_EVENT) ;

  accept_event_ = WSACreateEvent() ;
  int os_error = WSAGetLastError() ;
  if (accept_event_ == WSA_INVALID_EVENT) {
    printf("ERROR: WSACreateEvent() returned an error\n") ;
    return MapSystemError(os_error) ;
  }

  int result = listen(socket_, backlog) ;
  os_error = WSAGetLastError() ;
  if (result < 0) {
    printf("ERROR: listen() returned an error\n") ;
    return MapSystemError(os_error) ;
  }

  return vv::errOk ;
}

vv::Error TcpSocketWin::Accept(
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
    vv::Error net_error = vv::errTimeout ;
    if (result != WSA_WAIT_TIMEOUT) {
      net_error = MapSystemError(os_error) ;
    }

    if (net_error != vv::errTimeout) {
      printf("ERROR: WSAWaitForMultipleEvents() returned an error\n") ;
    }

    return net_error ;
  }

  SockaddrStorage storage ;
  SOCKET new_socket = accept(socket_, storage.addr, &storage.addr_len) ;
  os_error = WSAGetLastError() ;
  if (new_socket < 0) {
    int net_error = MapSystemError(os_error) ;
    printf("ERROR: accept() returned an error\n") ;
    return net_error ;
  }

  IPEndPoint ip_end_point ;
  if (!ip_end_point.FromSockAddr(storage.addr, storage.addr_len)) {
    // TODO: NOTREACHED();
    if (closesocket(new_socket) < 0) {
      printf("ERROR: closesocket() returned an error\n") ;
    }

    int net_error = vv::errNetAddressInvalid ;
    printf("ERROR: ip_end_point.FromSockAddr() returned a failure\n") ;
    return net_error;
  }

  std::unique_ptr<TcpSocketWin> tcp_socket(new TcpSocketWin()) ;
  vv::Error adopt_result =
      tcp_socket->AdoptConnectedSocket(new_socket, ip_end_point) ;
  if (adopt_result != vv::errOk) {
    printf("ERROR: tcp_socket->AdoptConnectedSocket() returned an error\n") ;
    return adopt_result ;
  }

  *socket = std::move(tcp_socket) ;
  *address = ip_end_point ;
  return vv::errOk ;
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

vv::Error TcpSocketWin::GetLocalAddress(IPEndPoint* address) const {
  // TODO: DCHECK(address) ;

  SockaddrStorage storage ;
  if (getsockname(socket_, storage.addr, &storage.addr_len)) {
    int os_error = WSAGetLastError() ;
    printf("ERROR: getsockname() returned an error\n") ;
    return MapSystemError(os_error) ;
  }
  if (!address->FromSockAddr(storage.addr, storage.addr_len)) {
    printf("ERROR: address->FromSockAddr() returned a failure\n") ;
    return vv::errNetAddressInvalid ;
  }

  return vv::errOk ;
}

int TcpSocketWin::GetPeerAddress(IPEndPoint* address) const {
  // TODO: DCHECK(address) ;
  if (!IsConnected()) {
    return vv::errNetSocketNotConnected ;
  }

  *address = *peer_address_ ;
  return vv::errOk ;
}

vv::Error TcpSocketWin::SetDefaultOptionsForServer() {
  return SetExclusiveAddrUse() ;
}

void TcpSocketWin::SetDefaultOptionsForClient() {
  SetTCPNoDelay(socket_, /*no_delay=*/true) ;
  SetTCPKeepAlive(socket_, true, kTCPKeepAliveSeconds) ;
}

vv::Error TcpSocketWin::SetExclusiveAddrUse() {
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
    return MapSystemError(errno) ;
  }

  return vv::errOk ;
}

vv::Error TcpSocketWin::SetReceiveBufferSize(std::int32_t size) {
  return SetSocketReceiveBufferSize(socket_, size) ;
}

vv::Error TcpSocketWin::SetSendBufferSize(std::int32_t size) {
  return SetSocketSendBufferSize(socket_, size) ;
}

bool TcpSocketWin::SetKeepAlive(bool enable, int delay) {
  return SetTCPKeepAlive(socket_, enable, delay) ;
}

bool TcpSocketWin::SetNoDelay(bool no_delay) {
  return SetTCPNoDelay(socket_, no_delay) == vv::errOk ;
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
    if (closesocket(socket_) < 0){
      printf("ERROR: closesocket() returned an error\n") ;
    }

    socket_ = INVALID_SOCKET ;
  }

  if (accept_event_ != WSA_INVALID_EVENT) {
    WSACloseEvent(accept_event_) ;
    accept_event_ = WSA_INVALID_EVENT ;
  }

  peer_address_.reset() ;
}
