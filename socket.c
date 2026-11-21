/* $Id: socket.c 1.1 1995/01/01 07:11:14 cthuang Exp $
 *
 * This module has been modified by Radim Kolar for OS/2 emx
 * This module has been modifide by Dave in Linux 2.6.35.14.i686
 */

/***********************************************************************
  module:       socket.c
  program:      popclient
  SCCS ID:      @(#)socket.c    1.5  4/1/94
  programmer:   Virginia Tech Computing Center
  compiler:     DEC RISC C compiler (Ultrix 4.1)
  environment:  DEC Ultrix 4.3
  description:  UNIX sockets code.

  Rewritten with getaddrinfo(): supports IPv4/IPv6 and is thread-safe.
 ***********************************************************************/
#include "socket.h"
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int Socket(const char *host, int clientPort) {
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  struct addrinfo *rp;
  char port[6];
  int sock = -1;

  if (clientPort <= 0 || clientPort > 65535)
    return -1;
  snprintf(port, sizeof port, "%d", clientPort);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; /* IPv4 与 IPv6 均可 */
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  if (getaddrinfo(host, port, &hints, &result) != 0)
    return -1;

  /* 依次尝试解析结果，直到某个地址连接成功 */
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock < 0)
      continue;
    if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    close(sock);
    sock = -1;
  }

  freeaddrinfo(result);
  return sock;
}
