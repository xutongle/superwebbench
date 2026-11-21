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
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static SSL_CTX *tls_ctx = NULL;

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

  tls_ctx = SSL_CTX_new(TLS_client_method());
  if (tls_ctx == NULL)
    return -1;
  if (SSL_CTX_set_default_verify_paths(tls_ctx) != 1) {
    SSL_CTX_free(tls_ctx);
    tls_ctx = NULL;
    return -1;
  }

  /* HTTPS 默认验证证书链，主机名或 IP 在每次连接时校验。 */
  SSL_CTX_set_verify(tls_ctx, SSL_VERIFY_PEER, NULL);
  SSL_CTX_set_mode(tls_ctx, SSL_MODE_AUTO_RETRY);
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
  /* 兼容以 TCP EOF 结束响应、但未发送 TLS close_notify 的服务器。 */
  SSL_CTX_set_options(tls_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif
  return 0;
}

void Connection_cleanup(void) {
  SSL_CTX_free(tls_ctx);
  tls_ctx = NULL;
}

void Connection_print_errors(void) { ERR_print_errors_fp(stderr); }

static int is_ip_address(const char *host) {
  unsigned char address[sizeof(struct in6_addr)];

  return inet_pton(AF_INET, host, address) == 1 ||
         inet_pton(AF_INET6, host, address) == 1;
}

int Connection_open(Connection *connection, const char *host, int clientPort,
                    int use_tls) {
  connection->fd = -1;
  connection->ssl = NULL;
  ERR_clear_error();

  connection->fd = Socket(host, clientPort);
  if (connection->fd < 0 || !use_tls)
    return connection->fd < 0 ? -1 : 0;
  if (tls_ctx == NULL)
    goto fail;

  connection->ssl = SSL_new(tls_ctx);
  if (connection->ssl == NULL || SSL_set_fd(connection->ssl, connection->fd) != 1)
    goto fail;

  if (is_ip_address(host)) {
    if (X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(connection->ssl), host) != 1)
      goto fail;
  } else {
    /* SNI 与证书主机名校验仅用于 DNS 主机名，不用于 IP 字面量。 */
    if (SSL_set_tlsext_host_name(connection->ssl, host) != 1 ||
        SSL_set1_host(connection->ssl, host) != 1)
      goto fail;
  }

  if (SSL_connect(connection->ssl) != 1)
    goto fail;
  return 0;

fail:
  SSL_free(connection->ssl);
  connection->ssl = NULL;
  if (connection->fd >= 0)
    close(connection->fd);
  connection->fd = -1;
  return -1;
}

ssize_t Connection_read(Connection *connection, void *buf, size_t size) {
  if (connection->ssl == NULL) {
    ssize_t ret;
    do {
      ret = read(connection->fd, buf, size);
    } while (ret < 0 && errno == EINTR);
    return ret;
  }

  for (;;) {
    int ret;
    int error;

    ERR_clear_error();
    errno = 0;
    ret = SSL_read(connection->ssl, buf, (int)size);
    if (ret > 0)
      return ret;
    error = SSL_get_error(connection->ssl, ret);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE ||
        (error == SSL_ERROR_SYSCALL && errno == EINTR))
      continue;
    if (error == SSL_ERROR_ZERO_RETURN ||
        (error == SSL_ERROR_SYSCALL && ret == 0))
      return 0;
    return -1;
  }
}

ssize_t Connection_write(Connection *connection, const void *buf, size_t size) {
  if (connection->ssl == NULL) {
    ssize_t ret;
    do {
      ret = write(connection->fd, buf, size);
    } while (ret < 0 && errno == EINTR);
    return ret;
  }

  for (;;) {
    int ret;
    int error;

    ERR_clear_error();
    errno = 0;
    ret = SSL_write(connection->ssl, buf, (int)size);
    if (ret > 0)
      return ret;
    error = SSL_get_error(connection->ssl, ret);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE ||
        (error == SSL_ERROR_SYSCALL && errno == EINTR))
      continue;
    return -1;
  }
}

int Connection_close(Connection *connection) {
  int ret = 0;

  if (connection->ssl != NULL) {
    ERR_clear_error();
    if (SSL_shutdown(connection->ssl) < 0)
      ret = -1;
    SSL_free(connection->ssl);
    connection->ssl = NULL;
  }
  if (connection->fd >= 0 && close(connection->fd) < 0)
    ret = -1;
  connection->fd = -1;
  return ret;
}
