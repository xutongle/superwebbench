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

  TLS 相关调用全部走 tls.h 抽象层，具体使用 OpenSSL 还是 mbed TLS
 由 configure 在编译期决定，本文件不再依赖任何具体 TLS 库的头文件。
 ***********************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "socket.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static TLS_CTX *tls_ctx = NULL;

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

int Connection_init(void) {
  if (tls_ctx != NULL)
    return 0;

  tls_ctx = TLS_CTX_create();
  return tls_ctx == NULL ? -1 : 0;
}

void Connection_cleanup(void) {
  TLS_CTX_destroy(tls_ctx);
  tls_ctx = NULL;
}

void Connection_print_errors(void) { TLS_print_errors(); }

int Connection_open(Connection *connection, const char *host, int clientPort,
                    int use_tls) {
  connection->fd = -1;
  connection->tls = NULL;

  connection->fd = Socket(host, clientPort);
  if (connection->fd < 0 || !use_tls)
    return connection->fd < 0 ? -1 : 0;
  if (tls_ctx == NULL)
    goto fail;

  connection->tls = TLS_create(tls_ctx, connection->fd);
  if (connection->tls == NULL)
    goto fail;
  if (TLS_set_peer_name(connection->tls, host) != 0)
    goto fail;
  if (TLS_handshake(connection->tls) != 0)
    goto fail;
  return 0;

fail:
  TLS_destroy(connection->tls);
  connection->tls = NULL;
  if (connection->fd >= 0)
    close(connection->fd);
  connection->fd = -1;
  return -1;
}

ssize_t Connection_read(Connection *connection, void *buf, size_t size) {
  if (connection->tls == NULL) {
    ssize_t ret;
    do {
      ret = read(connection->fd, buf, size);
    } while (ret < 0 && errno == EINTR);
    return ret;
  }
  return TLS_read(connection->tls, buf, size);
}

ssize_t Connection_write(Connection *connection, const void *buf, size_t size) {
  if (connection->tls == NULL) {
    ssize_t ret;
    do {
      ret = write(connection->fd, buf, size);
    } while (ret < 0 && errno == EINTR);
    return ret;
  }
  return TLS_write(connection->tls, buf, size);
}

int Connection_close(Connection *connection) {
  int ret = 0;

  if (connection->tls != NULL) {
    if (TLS_shutdown(connection->tls) != 0)
      ret = -1;
    TLS_destroy(connection->tls);
    connection->tls = NULL;
  }
  if (connection->fd >= 0 && close(connection->fd) < 0)
    ret = -1;
  connection->fd = -1;
  return ret;
}
