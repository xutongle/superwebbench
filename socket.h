#ifndef _WEBBENCH_SOCKET
#define _WEBBENCH_SOCKET

#include "tls.h"
#include <sys/types.h>

typedef struct {
  int fd;
  TLS *tls;
} Connection;

int Socket(const char *host, int clientPort);
int Connection_init(void);
void Connection_cleanup(void);
void Connection_print_errors(void);
int Connection_open(Connection *connection, const char *host, int clientPort,
                    int use_tls);
ssize_t Connection_read(Connection *connection, void *buf, size_t size);
ssize_t Connection_write(Connection *connection, const void *buf, size_t size);
int Connection_close(Connection *connection);
#endif
