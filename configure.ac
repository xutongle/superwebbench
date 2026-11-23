AC_PREREQ([2.69])
AC_INIT([superwebbench], [1.0.1])
AC_CONFIG_SRCDIR([superwebbench.c])
AC_CONFIG_HEADERS([config.h])
AC_PREFIX_DEFAULT([/usr])
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC
AC_PROG_INSTALL

AC_ARG_VAR([PKG_CONFIG], [path to the pkg-config utility])
AC_ARG_VAR([OPENSSL_CFLAGS], [C compiler flags for OpenSSL])
AC_ARG_VAR([OPENSSL_LIBS], [linker flags for OpenSSL])
AC_ARG_VAR([MBEDTLS_CFLAGS], [C compiler flags for mbed TLS])
AC_ARG_VAR([MBEDTLS_LIBS], [linker flags for mbed TLS])

AC_MSG_CHECKING([whether the compiler supports C11 atomics])
c11_saved_CFLAGS=$CFLAGS
C11_CFLAGS=
c11_supported=no
for c11_flag in "" "-std=c11" "-std=gnu11"; do
  CFLAGS="$c11_saved_CFLAGS $c11_flag"
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <stdatomic.h>]],
      [[_Atomic int value = 0;
        atomic_store(&value, 1);
        return atomic_load(&value) == 1 ? 0 : 1;]])],
    [c11_supported=yes
     C11_CFLAGS=$c11_flag
     break])
done
CFLAGS=$c11_saved_CFLAGS
AS_IF([test "x$c11_supported" = xyes],
  [AC_MSG_RESULT([${C11_CFLAGS:-yes}])],
  [AC_MSG_RESULT([no])
   AC_MSG_ERROR([a compiler with C11 stdatomic support is required])])
AC_SUBST([C11_CFLAGS])

AC_MSG_CHECKING([whether C11 atomics require an additional library])
atomic_saved_CFLAGS=$CFLAGS
atomic_saved_LIBS=$LIBS
CFLAGS="$CFLAGS $C11_CFLAGS"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <stdatomic.h>]],
    [[_Atomic int value = 0;
      atomic_store(&value, 1);
      return atomic_load(&value) == 1 ? 0 : 1;]])],
  [ATOMIC_LIBS=
   AC_MSG_RESULT([none needed])],
  [LIBS="-latomic $atomic_saved_LIBS"
   AC_LINK_IFELSE(
     [AC_LANG_PROGRAM(
       [[#include <stdatomic.h>]],
       [[_Atomic int value = 0;
         atomic_store(&value, 1);
         return atomic_load(&value) == 1 ? 0 : 1;]])],
     [ATOMIC_LIBS=-latomic
      AC_MSG_RESULT([-latomic])],
     [AC_MSG_RESULT([no])
      AC_MSG_ERROR([C11 atomic operations could not be linked])])])
CFLAGS=$atomic_saved_CFLAGS
LIBS=$atomic_saved_LIBS
AC_SUBST([ATOMIC_LIBS])

pthread_test_program='
#include <pthread.h>
int main(void) {
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 32768);
  pthread_create(&thread, &attr, (void *(*)(void *))0, (void *)0);
  pthread_join(thread, (void **)0);
  pthread_attr_destroy(&attr);
  return 0;
}
'
AC_MSG_CHECKING([how to compile and link POSIX threads])
pthread_saved_CFLAGS=$CFLAGS
pthread_saved_LIBS=$LIBS
PTHREAD_CFLAGS=
PTHREAD_LIBS=
pthread_mode=
CFLAGS="$CFLAGS $C11_CFLAGS -pthread"
LIBS="$pthread_saved_LIBS -pthread"
AC_LINK_IFELSE([AC_LANG_SOURCE([$pthread_test_program])], [pthread_mode=pthread])
AS_IF([test -z "$pthread_mode"], [
  CFLAGS="$pthread_saved_CFLAGS $C11_CFLAGS"
  LIBS="$pthread_saved_LIBS -lpthread"
  AC_LINK_IFELSE([AC_LANG_SOURCE([$pthread_test_program])], [pthread_mode=lpthread])
])
AS_IF([test -z "$pthread_mode"], [
  CFLAGS="$pthread_saved_CFLAGS $C11_CFLAGS"
  LIBS=$pthread_saved_LIBS
  AC_LINK_IFELSE([AC_LANG_SOURCE([$pthread_test_program])], [pthread_mode=libc])
])
AS_CASE([$pthread_mode],
  [pthread], [PTHREAD_CFLAGS=-pthread
              PTHREAD_LIBS=-pthread
              AC_MSG_RESULT([-pthread])],
  [lpthread], [PTHREAD_LIBS=-lpthread
               AC_MSG_RESULT([-lpthread])],
  [libc], [AC_MSG_RESULT([none needed])],
  [AC_MSG_RESULT([no])
   AC_MSG_ERROR([POSIX threads with pthread_create, pthread_join, and pthread_attr_setstacksize are required])])
CFLAGS=$pthread_saved_CFLAGS
LIBS=$pthread_saved_LIBS
AC_SUBST([PTHREAD_CFLAGS])
AC_SUBST([PTHREAD_LIBS])

AC_CHECK_HEADERS([arpa/inet.h getopt.h netdb.h strings.h sys/resource.h sys/socket.h unistd.h], [],
  [AC_MSG_ERROR([required header $ac_header was not found])])

AC_SEARCH_LIBS([socket], [socket], [],
  [AC_MSG_ERROR([the socket function is required])])
AC_SEARCH_LIBS([getaddrinfo], [nsl socket], [],
  [AC_MSG_ERROR([getaddrinfo is required])])
AC_SEARCH_LIBS([inet_pton], [nsl socket], [],
  [AC_MSG_ERROR([inet_pton is required])])
AC_SEARCH_LIBS([getopt_long], [gnugetopt], [],
  [AC_MSG_ERROR([getopt_long is required])])

AC_MSG_CHECKING([for IPv6 address resolution support])
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <arpa/inet.h>
      #include <netdb.h>
      #include <sys/socket.h>]],
    [[struct addrinfo hints = {0};
      struct addrinfo *result = 0;
      struct in6_addr address;
      hints.ai_family = AF_UNSPEC;
      inet_pton(AF_INET6, "::1", &address);
      getaddrinfo("localhost", "80", &hints, &result);
      freeaddrinfo(result);]])],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])
   AC_MSG_ERROR([getaddrinfo, freeaddrinfo, inet_pton, and IPv6 socket types are required])])

AC_CHECK_FUNCS([getrlimit setrlimit strncasecmp usleep], [],
  [AC_MSG_ERROR([required function $ac_func was not found])])
AC_CHECK_DECLS([RLIMIT_NOFILE, RLIMIT_NPROC], [], [],
  [[#include <sys/resource.h>]])
AS_IF([test "x$ac_cv_have_decl_RLIMIT_NOFILE" != xyes],
  [AC_MSG_ERROR([RLIMIT_NOFILE is required])])

AC_PATH_TOOL([PKG_CONFIG], [pkg-config])

dnl ###########################################################################
dnl TLS 后端选择：默认优先使用 OpenSSL，在 OpenSSL 不可用时回退到 mbed TLS。
dnl 可用 --with-tls=auto|openssl|mbedtls 显式指定（默认 auto）。
dnl ###########################################################################
AC_ARG_WITH([tls],
  [AS_HELP_STRING([--with-tls=BACKEND],
     [TLS backend to use: 'auto' (default, prefer OpenSSL then mbed TLS), 'openssl', or 'mbedtls'])],
  [with_tls=$withval], [with_tls=auto])
AS_CASE([$with_tls], [auto | openssl | mbedtls], [],
  [AC_MSG_ERROR([invalid --with-tls value '$with_tls'; use auto, openssl, or mbedtls])])

dnl 检测 OpenSSL：成功时设置 swb_tls_ok=yes，并填充 OPENSSL_CFLAGS / OPENSSL_LIBS。
swb_check_openssl () {
  swb_tls_ok=no
  AS_IF([test -n "$PKG_CONFIG" && command -v "$PKG_CONFIG" >/dev/null 2>&1], [
    AS_IF(["$PKG_CONFIG" --exists openssl], [
      AS_IF([test -z "$OPENSSL_CFLAGS"],
        [OPENSSL_CFLAGS=`"$PKG_CONFIG" --cflags openssl`])
      AS_IF([test -z "$OPENSSL_LIBS"],
        [OPENSSL_LIBS=`"$PKG_CONFIG" --libs openssl`])
    ])
  ])
  AS_IF([test -z "$OPENSSL_LIBS"], [OPENSSL_LIBS="-lssl -lcrypto"])

  AC_MSG_CHECKING([whether OpenSSL provides the required TLS and certificate verification APIs])
  swb_saved_CPPFLAGS=$CPPFLAGS
  swb_saved_LIBS=$LIBS
  CPPFLAGS="$OPENSSL_CFLAGS $CPPFLAGS"
  LIBS="$OPENSSL_LIBS $LIBS"
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <openssl/ssl.h>
        #include <openssl/x509v3.h>]],
      [[SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        SSL *ssl = SSL_new(ctx);
        X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
        SSL_CTX_set_default_verify_paths(ctx);
        SSL_set_tlsext_host_name(ssl, "localhost");
        SSL_set1_host(ssl, "localhost");
        X509_VERIFY_PARAM_set1_ip_asc(param, "127.0.0.1");
        SSL_free(ssl);
        SSL_CTX_free(ctx);]])],
    [AC_MSG_RESULT([yes])
     swb_tls_ok=yes],
    [AC_MSG_RESULT([no])])
  CPPFLAGS=$swb_saved_CPPFLAGS
  LIBS=$swb_saved_LIBS
}

dnl 检测 mbed TLS：成功时设置 swb_tls_ok=yes，并填充 MBEDTLS_CFLAGS / MBEDTLS_LIBS。
dnl 兼容 2.x / 3.x / 4.x；动态库未传递私有依赖时回退到 pkg-config --static。
swb_check_mbedtls () {
  swb_tls_ok=no
  AS_IF([test -n "$PKG_CONFIG" && command -v "$PKG_CONFIG" >/dev/null 2>&1], [
    AS_IF(["$PKG_CONFIG" --exists mbedtls], [
      AS_IF([test -z "$MBEDTLS_CFLAGS"],
        [MBEDTLS_CFLAGS=`"$PKG_CONFIG" --cflags mbedtls`])
      AS_IF([test -z "$MBEDTLS_LIBS"],
        [MBEDTLS_LIBS=`"$PKG_CONFIG" --libs mbedtls`])
    ])
  ])
  AS_IF([test -z "$MBEDTLS_LIBS"],
    [MBEDTLS_LIBS="-lmbedtls -lmbedx509 -lmbedcrypto"])

  AC_MSG_CHECKING([whether mbed TLS provides the required TLS and X.509 APIs])
  swb_saved_CPPFLAGS=$CPPFLAGS
  swb_saved_LIBS=$LIBS
  CPPFLAGS="$MBEDTLS_CFLAGS $CPPFLAGS"
  LIBS="$MBEDTLS_LIBS $LIBS"
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <mbedtls/version.h>
        #include <mbedtls/ssl.h>
        #include <mbedtls/x509_crt.h>]],
      [[mbedtls_ssl_config conf;
        mbedtls_ssl_config_init(&conf);
        mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_x509_crt ca;
        mbedtls_x509_crt_init(&ca);
        mbedtls_x509_crt_parse_file(&ca, "/dev/null");
        mbedtls_x509_crt_free(&ca);
        mbedtls_ssl_config_free(&conf);
        return 0;]])],
    [AC_MSG_RESULT([yes])
     swb_tls_ok=yes],
    [AC_MSG_RESULT([no])
     AS_IF([test -n "$PKG_CONFIG" && command -v "$PKG_CONFIG" >/dev/null 2>&1 && "$PKG_CONFIG" --exists mbedtls], [
       dnl 动态库可能没有传递私有依赖，改用 --static 取出完整依赖再试一次。
       MBEDTLS_LIBS=`"$PKG_CONFIG" --libs --static mbedtls`
       LIBS="$MBEDTLS_LIBS $swb_saved_LIBS"
       AC_LINK_IFELSE(
         [AC_LANG_PROGRAM(
           [[#include <mbedtls/version.h>
             #include <mbedtls/ssl.h>]],
           [[mbedtls_ssl_config conf;
             mbedtls_ssl_config_init(&conf);
             mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT);
             return 0;]])],
         [swb_tls_ok=yes], [])
     ])])
  CPPFLAGS=$swb_saved_CPPFLAGS
  LIBS=$swb_saved_LIBS
}

dnl 按 --with-tls 的选择执行检测。
AS_CASE([$with_tls],
  [openssl],
    [swb_check_openssl
     AS_IF([test "x$swb_tls_ok" = xyes], [swb_tls_backend=openssl],
       [AC_MSG_ERROR([OpenSSL with TLS client, SNI, hostname, and IP verification APIs is required; set OPENSSL_CFLAGS/OPENSSL_LIBS for a non-standard installation])])],
  [mbedtls],
    [swb_check_mbedtls
     AS_IF([test "x$swb_tls_ok" = xyes], [swb_tls_backend=mbedtls],
       [AC_MSG_ERROR([mbed TLS with TLS client and X.509 APIs is required; install mbed TLS 2.x/3.x/4.x])])],
  [auto],
    [swb_check_openssl
     AS_IF([test "x$swb_tls_ok" = xyes], [swb_tls_backend=openssl],
       [AC_MSG_NOTICE([OpenSSL not available; trying mbed TLS as TLS backend])
        swb_check_mbedtls
        AS_IF([test "x$swb_tls_ok" = xyes], [swb_tls_backend=mbedtls],
          [AC_MSG_ERROR([neither OpenSSL nor mbed TLS was found; install one of them, or set OPENSSL_CFLAGS/OPENSSL_LIBS or MBEDTLS_CFLAGS/MBEDTLS_LIBS])])])])

dnl 根据选中的后端定义编译宏与构建变量。
AS_IF([test "x$swb_tls_backend" = xopenssl], [
  AC_DEFINE([HAVE_OPENSSL], [1], [Define to 1 if OpenSSL is the selected TLS backend])
  TLS_OBJS=tls_openssl.o
  TLS_CFLAGS="$OPENSSL_CFLAGS"
  TLS_LIBS="$OPENSSL_LIBS"
  TLS_BACKEND=OpenSSL
], [
  AC_DEFINE([HAVE_MBEDTLS], [1], [Define to 1 if mbed TLS is the selected TLS backend])
  TLS_OBJS=tls_mbedtls.o
  TLS_CFLAGS="$MBEDTLS_CFLAGS"
  TLS_LIBS="$MBEDTLS_LIBS"
  TLS_BACKEND=mbedTLS
])
AC_SUBST([OPENSSL_CFLAGS])
AC_SUBST([OPENSSL_LIBS])
AC_SUBST([MBEDTLS_CFLAGS])
AC_SUBST([MBEDTLS_LIBS])
AC_SUBST([TLS_OBJS])
AC_SUBST([TLS_CFLAGS])
AC_SUBST([TLS_LIBS])
AC_SUBST([TLS_BACKEND])
AC_MSG_NOTICE([selected TLS backend: $TLS_BACKEND])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
