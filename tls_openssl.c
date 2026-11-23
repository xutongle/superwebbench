/*
 * tls_openssl.c - 基于 OpenSSL 的 TLS 后端实现
 *
 * 仅在 configure 选中 OpenSSL 后端时参与编译。实现 tls.h 中定义的
 * 统一接口，把 OpenSSL 特有的 API 与错误处理封装在内部。
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tls.h"

#include <arpa/inet.h>
#include <errno.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <stdio.h>
#include <string.h>

struct tls_ctx_st {
  SSL_CTX *ssl_ctx;
};

struct tls_st {
  SSL *ssl;
};

/* 判断 host 是否为 IPv4/IPv6 字面量。 */
static int is_ip_address(const char *host) {
  unsigned char address[sizeof(struct in6_addr)];

  return inet_pton(AF_INET, host, address) == 1 ||
         inet_pton(AF_INET6, host, address) == 1;
}

TLS_CTX *TLS_CTX_create(void) {
  TLS_CTX *ctx;

  ctx = (TLS_CTX *)calloc(1, sizeof(*ctx));
  if (ctx == NULL) {
    fprintf(stderr, "TLS: out of memory while creating OpenSSL context\n");
    return NULL;
  }

  ctx->ssl_ctx = SSL_CTX_new(TLS_client_method());
  if (ctx->ssl_ctx == NULL) {
    fprintf(stderr, "TLS: failed to create OpenSSL context\n");
    TLS_print_errors();
    free(ctx);
    return NULL;
  }

  if (SSL_CTX_set_default_verify_paths(ctx->ssl_ctx) != 1) {
    fprintf(stderr, "TLS: failed to load default CA verification paths\n");
    TLS_print_errors();
    SSL_CTX_free(ctx->ssl_ctx);
    free(ctx);
    return NULL;
  }

  /* HTTPS 默认验证证书链；主机名或 IP 在每次连接时校验。 */
  SSL_CTX_set_verify(ctx->ssl_ctx, SSL_VERIFY_PEER, NULL);
  SSL_CTX_set_mode(ctx->ssl_ctx, SSL_MODE_AUTO_RETRY);
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
  /* 兼容以 TCP EOF 结束响应、但未发送 TLS close_notify 的服务器。 */
  SSL_CTX_set_options(ctx->ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif
  return ctx;
}

void TLS_CTX_destroy(TLS_CTX *ctx) {
  if (ctx == NULL)
    return;
  SSL_CTX_free(ctx->ssl_ctx);
  free(ctx);
}

TLS *TLS_create(TLS_CTX *ctx, int fd) {
  TLS *tls;

  if (ctx == NULL || ctx->ssl_ctx == NULL)
    return NULL;

  tls = (TLS *)calloc(1, sizeof(*tls));
  if (tls == NULL)
    return NULL;

  tls->ssl = SSL_new(ctx->ssl_ctx);
  if (tls->ssl == NULL) {
    free(tls);
    return NULL;
  }
  if (SSL_set_fd(tls->ssl, fd) != 1) {
    SSL_free(tls->ssl);
    free(tls);
    return NULL;
  }
  return tls;
}

void TLS_destroy(TLS *tls) {
  if (tls == NULL)
    return;
  SSL_free(tls->ssl);
  free(tls);
}

int TLS_set_peer_name(TLS *tls, const char *host) {
  X509_VERIFY_PARAM *param;

  if (tls == NULL || tls->ssl == NULL || host == NULL)
    return -1;

  if (is_ip_address(host)) {
    /* 对 IP 字面量做证书 IP 校验；不设置 SNI。 */
    param = SSL_get0_param(tls->ssl);
    return X509_VERIFY_PARAM_set1_ip_asc(param, host) == 1 ? 0 : -1;
  }

  /* DNS 主机名：设置 SNI 并启用证书主机名校验。 */
  if (SSL_set_tlsext_host_name(tls->ssl, host) != 1)
    return -1;
  return SSL_set1_host(tls->ssl, host) == 1 ? 0 : -1;
}

int TLS_handshake(TLS *tls) {
  if (tls == NULL || tls->ssl == NULL)
    return -1;
  return SSL_connect(tls->ssl) == 1 ? 0 : -1;
}

ssize_t TLS_read(TLS *tls, void *buf, size_t size) {
  for (;;) {
    int ret;
    int error;

    ERR_clear_error();
    errno = 0;
    ret = SSL_read(tls->ssl, buf, (int)size);
    if (ret > 0)
      return (ssize_t)ret;
    error = SSL_get_error(tls->ssl, ret);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE ||
        (error == SSL_ERROR_SYSCALL && errno == EINTR))
      continue;
    if (error == SSL_ERROR_ZERO_RETURN ||
        (error == SSL_ERROR_SYSCALL && ret == 0))
      return 0;
    return -1;
  }
}

ssize_t TLS_write(TLS *tls, const void *buf, size_t size) {
  for (;;) {
    int ret;
    int error;

    ERR_clear_error();
    errno = 0;
    ret = SSL_write(tls->ssl, buf, (int)size);
    if (ret > 0)
      return (ssize_t)ret;
    error = SSL_get_error(tls->ssl, ret);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE ||
        (error == SSL_ERROR_SYSCALL && errno == EINTR))
      continue;
    return -1;
  }
}

int TLS_shutdown(TLS *tls) {
  if (tls == NULL || tls->ssl == NULL)
    return 0;
  ERR_clear_error();
  return SSL_shutdown(tls->ssl) < 0 ? -1 : 0;
}

void TLS_print_errors(void) { ERR_print_errors_fp(stderr); }

const char *TLS_backend_name(void) { return "OpenSSL"; }
