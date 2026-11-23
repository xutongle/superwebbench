/*
 * tls_mbedtls.c - 基于 mbed TLS 的 TLS 后端实现
 *
 * 仅在 configure 选中 mbed TLS 后端时参与编译。兼容 mbed TLS 2.x / 3.x / 4.x：
 *   - 4.x 起移除了 CTR_DRBG / ENTROPY 模块与 mbedtls_ssl_conf_rng()，
 *     改用 PSA RNG（需先调用 psa_crypto_init()）。
 *   - 2.x / 3.x 仍使用 mbedtls_ctr_drbg + mbedtls_entropy，并通过
 *     mbedtls_ssl_conf_rng() 注入，随机数回调用互斥锁保护以保证线程安全。
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tls.h"

/* version.h 在所有版本都定义（或间接包含）MBEDTLS_VERSION_MAJOR。 */
#include <mbedtls/version.h>

#if MBEDTLS_VERSION_MAJOR >= 4
#include <psa/crypto.h> /* psa_crypto_init, PSA_SUCCESS, mbedtls_psa_crypto_free */
#else
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#endif

#include <errno.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct tls_ctx_st {
  mbedtls_x509_crt ca_chain;
  mbedtls_ssl_config conf;
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
#endif
};

struct tls_st {
  mbedtls_ssl_context ssl;
  int fd; /* bio 回调需要访问底层 fd */
};

/* 记录当前线程最近一次 mbed TLS 错误码，供 TLS_print_errors() 使用。 */
static __thread int tls_last_error = 0;

static void tls_set_error(int err) { tls_last_error = err; }

/* ---- 系统 CA 根证书加载 ---- */

static int file_exists(const char *path) {
  return path != NULL && access(path, R_OK) == 0;
}

static int load_system_ca(mbedtls_x509_crt *ca) {
  /* 常见 CA bundle 文件路径，覆盖 Debian/Ubuntu、RHEL 系、SUSE、macOS/BSD。 */
  const char *files[] = {
      getenv("SSL_CERT_FILE"),
      "/etc/ssl/certs/ca-certificates.crt", /* Debian/Ubuntu */
      "/etc/pki/tls/certs/ca-bundle.crt",   /* RHEL/CentOS/Fedora */
      "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", /* RHEL 新路径 */
      "/etc/ssl/ca-bundle.pem",             /* OpenSUSE */
      "/etc/ssl/cert.pem",                  /* macOS/BSD */
      "/usr/local/etc/ssl/cert.pem",
      "/usr/share/ssl/certs/ca-bundle.crt",
      NULL,
  };
  /* 按目录加载（哈希链接目录）。 */
  const char *dirs[] = {
      getenv("SSL_CERT_DIR"),
      "/etc/ssl/certs",     /* Debian/Ubuntu */
      "/etc/pki/tls/certs", /* RHEL 系 */
      "/usr/share/ssl/certs",
      NULL,
  };
  size_t i;
  int ret;

  for (i = 0; files[i] != NULL; i++) {
    if (!file_exists(files[i]))
      continue;
    ret = mbedtls_x509_crt_parse_file(ca, files[i]);
    if (ret >= 0)
      return 0; /* 成功解析（返回值为跳过的条目数） */
  }
  for (i = 0; dirs[i] != NULL; i++) {
    if (!file_exists(dirs[i]))
      continue;
    ret = mbedtls_x509_crt_parse_path(ca, dirs[i]);
    if (ret >= 0)
      return 0;
  }
  return -1;
}

/* ---- BIO 回调：用 socket fd 做 recv/send ---- */

static int bio_recv(void *ctx, unsigned char *buf, size_t len) {
  int fd = *(int *)ctx;

  for (;;) {
    ssize_t n = recv(fd, buf, len, 0);
    if (n > 0)
      return (int)n;
    if (n == 0)
      return 0; /* 对端关闭，mbed TLS 内部转为 CONN_EOF */
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return MBEDTLS_ERR_SSL_WANT_READ;
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
}

static int bio_send(void *ctx, const unsigned char *buf, size_t len) {
  int fd = *(int *)ctx;

  for (;;) {
    ssize_t n = send(fd, buf, len, 0);
    if (n > 0)
      return (int)n;
    if (n == 0)
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
}

/* ---- 2.x / 3.x 专用：线程安全的 CTR_DRBG 随机回调 ---- */

#if MBEDTLS_VERSION_MAJOR < 4
static pthread_mutex_t tls_rng_mutex = PTHREAD_MUTEX_INITIALIZER;

static int tls_ctr_drbg_random(void *p_rng, unsigned char *out, size_t len) {
  int ret;
  pthread_mutex_lock(&tls_rng_mutex);
  ret = mbedtls_ctr_drbg_random(p_rng, out, len);
  pthread_mutex_unlock(&tls_rng_mutex);
  return ret;
}
#endif

/* ---- 接口实现 ---- */

TLS_CTX *TLS_CTX_create(void) {
  TLS_CTX *ctx;
  int ret;

  ctx = (TLS_CTX *)calloc(1, sizeof(*ctx));
  if (ctx == NULL) {
    fprintf(stderr, "TLS: out of memory while creating mbed TLS context\n");
    return NULL;
  }

#if MBEDTLS_VERSION_MAJOR >= 4
  {
    psa_status_t st = psa_crypto_init();
    if (st != PSA_SUCCESS) {
      fprintf(stderr, "TLS: psa_crypto_init failed: %d\n", (int)st);
      free(ctx);
      return NULL;
    }
  }
#endif

  mbedtls_x509_crt_init(&ctx->ca_chain);
  mbedtls_ssl_config_init(&ctx->conf);

  if (load_system_ca(&ctx->ca_chain) != 0) {
    fprintf(stderr,
            "TLS: warning: could not load any system CA certificates; "
            "HTTPS certificate verification will fail.\n"
            "     Set SSL_CERT_FILE / SSL_CERT_DIR or install a CA bundle.\n");
  }

  ret = mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    tls_set_error(ret);
    fprintf(stderr, "TLS: mbedtls_ssl_config_defaults failed\n");
    goto fail;
  }
  mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->ca_chain, NULL);

#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_entropy_init(&ctx->entropy);
  mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
  ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func,
                              &ctx->entropy,
                              (const unsigned char *)"superwebbench", 13);
  if (ret != 0) {
    tls_set_error(ret);
    fprintf(stderr, "TLS: mbedtls_ctr_drbg_seed failed\n");
    goto fail;
  }
  mbedtls_ssl_conf_rng(&ctx->conf, tls_ctr_drbg_random, &ctx->ctr_drbg);
#endif

  return ctx;

fail:
  mbedtls_ssl_config_free(&ctx->conf);
  mbedtls_x509_crt_free(&ctx->ca_chain);
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
  mbedtls_entropy_free(&ctx->entropy);
#else
  mbedtls_psa_crypto_free();
#endif
  free(ctx);
  return NULL;
}

void TLS_CTX_destroy(TLS_CTX *ctx) {
  if (ctx == NULL)
    return;
  mbedtls_ssl_config_free(&ctx->conf);
  mbedtls_x509_crt_free(&ctx->ca_chain);
#if MBEDTLS_VERSION_MAJOR < 4
  mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
  mbedtls_entropy_free(&ctx->entropy);
#else
  mbedtls_psa_crypto_free();
#endif
  free(ctx);
}

TLS *TLS_create(TLS_CTX *ctx, int fd) {
  TLS *tls;
  int ret;

  if (ctx == NULL || fd < 0)
    return NULL;

  tls = (TLS *)calloc(1, sizeof(*tls));
  if (tls == NULL)
    return NULL;
  tls->fd = fd;
  mbedtls_ssl_init(&tls->ssl);

  ret = mbedtls_ssl_setup(&tls->ssl, &ctx->conf);
  if (ret != 0) {
    tls_set_error(ret);
    mbedtls_ssl_free(&tls->ssl);
    free(tls);
    return NULL;
  }
  mbedtls_ssl_set_bio(&tls->ssl, &tls->fd, bio_send, bio_recv, NULL);
  return tls;
}

void TLS_destroy(TLS *tls) {
  if (tls == NULL)
    return;
  mbedtls_ssl_free(&tls->ssl);
  free(tls);
}

int TLS_set_peer_name(TLS *tls, const char *host) {
  int ret;

  if (tls == NULL || host == NULL)
    return -1;
  /*
   * mbedtls_ssl_set_hostname 同时设置 SNI 扩展并在握手时校验
   * 证书中的主机名；对 IPv4/IPv6 字面量也能正确做 IP 校验。
   */
  ret = mbedtls_ssl_set_hostname(&tls->ssl, host);
  if (ret != 0) {
    tls_set_error(ret);
    return -1;
  }
  return 0;
}

int TLS_handshake(TLS *tls) {
  int ret;

  if (tls == NULL)
    return -1;
  do {
    ret = mbedtls_ssl_handshake(&tls->ssl);
  } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
           ret == MBEDTLS_ERR_SSL_WANT_WRITE);
  if (ret == 0)
    return 0;
  tls_set_error(ret);
  return -1;
}

ssize_t TLS_read(TLS *tls, void *buf, size_t size) {
  int ret;

  if (tls == NULL)
    return -1;
  do {
    ret = mbedtls_ssl_read(&tls->ssl, buf, size);
  } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
           ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  if (ret > 0)
    return (ssize_t)ret;
  if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
      ret == MBEDTLS_ERR_SSL_CONN_EOF)
    return 0; /* 对端干净关闭 */
  tls_set_error(ret);
  return -1;
}

ssize_t TLS_write(TLS *tls, const void *buf, size_t size) {
  int ret;

  if (tls == NULL)
    return -1;
  do {
    ret = mbedtls_ssl_write(&tls->ssl, buf, size);
  } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
           ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  if (ret > 0)
    return (ssize_t)ret;
  if (ret == 0)
    return 0;
  tls_set_error(ret);
  return -1;
}

int TLS_shutdown(TLS *tls) {
  int ret;

  if (tls == NULL)
    return 0;
  do {
    ret = mbedtls_ssl_close_notify(&tls->ssl);
  } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
           ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
    return 0;
  tls_set_error(ret);
  return -1;
}

void TLS_print_errors(void) {
  int err = tls_last_error;

  if (err == 0)
    return;
#ifdef MBEDTLS_ERROR_C
  {
    char buf[128];
    mbedtls_strerror(err, buf, sizeof(buf));
    fprintf(stderr, "TLS: %s (-0x%04x)\n", buf, (unsigned)-err);
  }
#else
  fprintf(stderr, "TLS: mbed TLS error -0x%04x\n", (unsigned)-err);
#endif
  tls_last_error = 0;
}

const char *TLS_backend_name(void) { return "mbed TLS"; }
