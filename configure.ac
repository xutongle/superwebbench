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
openssl_saved_CPPFLAGS=$CPPFLAGS
openssl_saved_LIBS=$LIBS
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
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])
   CPPFLAGS=$openssl_saved_CPPFLAGS
   LIBS=$openssl_saved_LIBS
   AC_MSG_ERROR([OpenSSL 1.1 or later with TLS client, SNI, hostname, and IP verification APIs is required; set OPENSSL_CFLAGS and OPENSSL_LIBS for a non-standard installation])])
CPPFLAGS=$openssl_saved_CPPFLAGS
LIBS=$openssl_saved_LIBS
AC_SUBST([OPENSSL_CFLAGS])
AC_SUBST([OPENSSL_LIBS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
