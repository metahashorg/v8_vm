// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "vm_apps/http_server/http-server-session.h"
#include "vm_apps/http_server/tcp-server.h"
#include "vm_apps/utils/command-line.h"

const char kSwitchPort[] = "port" ;

void HowToUse() {
  printf("ERROR: Specify a port of http-server\n") ;
}

int main(int argc, char* argv[]) {
  CommandLine cmd_line(argc, argv) ;
  if (!cmd_line.HasSwitch(kSwitchPort)) {
    HowToUse() ;
    return 1 ;
  }

  std::uint16_t server_port = StringToUint16(
      cmd_line.GetSwitchValueNative(kSwitchPort).c_str()) ;

  TcpServer server ;
  vv::Error error = server.Start(server_port, HttpServerSession::New) ;
  V8_ERR_RETURN_IF_FAILED(error) ;

  char сommand(0) ;
  while (сommand != 'q' && сommand != 'Q') {
    std::cin >> сommand ;
  }

  error = server.Stop() ;
  V8_ERR_RETURN_IF_FAILED(error) ;

  error = server.Wait() ;
  V8_ERR_RETURN_IF_FAILED(error) ;

  return 0 ;
}
