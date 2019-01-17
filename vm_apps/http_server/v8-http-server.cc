// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "vm_apps/http_server/http-server-session.h"
#include "vm_apps/http_server/tcp-server.h"
#include "vm_apps/http_server/v8-http-server-session.h"
#include "vm_apps/utils/command-line.h"
#include "vm_apps/utils/string-number-conversions.h"

namespace {

// Switches
const char kSwitchPort[] = "port" ;

// Server default parameters
const char kServerName[] = "v8-http-server/1.0" ;
const std::int32_t kBodyBufferSize = 256 * 1024 ; // because of snapshots

// Wrapper for to initialize V8
class V8Initializer {
 public:
  V8Initializer(const char* app_path) {
    vv::InitializeV8(app_path) ;
  }

  ~V8Initializer() {
    vv::DeinitializeV8() ;
  }
};

}  //namespace

void HowToUse() {
  printf("ERROR: Specify a port of http-server\n") ;
}

int main(int argc, char* argv[]) {
  CommandLine cmd_line(argc, argv) ;
  if (!cmd_line.HasSwitch(kSwitchPort)) {
    HowToUse() ;
    return 1 ;
  }

  std::uint16_t server_port = 0 ;
  if (!StringToUint16(
          cmd_line.GetSwitchValueNative(kSwitchPort).c_str(), &server_port)) {
    printf("ERROR: Server port is ivalid (Port: %s)\n",
           cmd_line.GetSwitchValueNative(kSwitchPort).c_str()) ;
    return errInvalidArgument ;
  }

  // Initialize V8
  V8Initializer v8_initializer(cmd_line.GetProgram().c_str()) ;

  // Start server
  TcpServer server ;
  Error error = server.Start(
      server_port,
      HttpServerSession::GetCreator(
          V8HttpServerSession::ProcessSession, kServerName, kBodyBufferSize)) ;
  V8_ERROR_RETURN_IF_FAILED(error) ;

  char command(0) ;
  while (command != 'q' && command != 'Q') {
    std::cin >> command ;
  }

  // Stop server
  error = server.Stop() ;
  V8_ERROR_RETURN_IF_FAILED(error) ;

  // Wait for server has stopped
  error = server.Wait() ;
  V8_ERROR_RETURN_IF_FAILED(error) ;

  return 0 ;
}
