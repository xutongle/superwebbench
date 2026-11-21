/*
 * Modified by XuTongle 2012-01-20
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple threaded WWW Server benchmark:
 *
 * Usage:
 *   superwebbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, pthread failed
 *    4 - resourse limits
 *
 */
#include "socket.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PROGRAM_VERSION "1.0.1"
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define HTTP_09 0
#define HTTP_10 1
#define HTTP_11 2
#define OUTPUT_TEXT 0
#define OUTPUT_JSON 1

#define BUF_SIZE 1024
#define URL_SIZE 1500
#define REQUEST_SIZE 2048
#define START_SLEEP 10000

#define DEFAULT_PROXY 0
#define DEFAULT_CLIENT 1
#define DEFAULT_TIME 30
#define DEFAULT_FORCE 0
#define DEFAULT_HTTPS 0
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443
#define DEFAULT_METHOD METHOD_GET
#define DEFAULT_OUTPUT OUTPUT_TEXT

#define DEFAULT_FORE_RELOAD 0
#define DEFAULT_STACK_SIZE 32768
#define DEFAULT_HTTP_VER HTTP_11
/* globals */
struct parameter {
  int force;
  int force_reload;
  int proxy;
  int https;
  int http_ver;
  int method;
  int port;
  int clients;
  int bench_time;
  int output;
  char *host;
  char *request;
};
struct thread_arg {
  char *host;
  char *request;
  int force;
  int https;
  int port;
  int http_ver;
  unsigned long speed;
  unsigned long failed;
  unsigned long httperr;
  unsigned long bytes;
  pthread_t thread;
  int started;
};
/* 压测停止标志：主线程到时置位，工作线程在每轮请求前检查 */
static _Atomic int bench_stop = 0;
static char const *const gs_http_method[] = {
    "GET",
    "HEAD",
    "OPTIONS",
    "TRACE",
};
static char const *const gs_http_version[] = {
    "HTTP/0.9",
    "HTTP/1.0",
    "HTTP/1.1",
};

/* prototypes */
static void *bench_thread(void *para);
static int bench(struct parameter *para, const char *url);
static void build_request(const char *url, struct parameter *para);
static int http_response_check(const char *response);
static int parse_opt(int argc, char *argv[], struct parameter *para);
static int resource_set(int clients);
static void json_string(const char *value);
static void print_result(const struct parameter *para, const char *url,
                         unsigned long long speed,
                         unsigned long long failed,
                         unsigned long long httperr,
                         unsigned long long bytes);

static int resource_set(int clients) {
  struct rlimit lim;
  static const int used_lim = 128;

  getrlimit(RLIMIT_NOFILE, &lim);                    // get file des limit
  if (lim.rlim_cur < (rlim_t)(used_lim + clients)) { // set file des limit
    lim.rlim_cur = used_lim + clients;
    lim.rlim_max = used_lim + clients;
    if (setrlimit(RLIMIT_NOFILE, &lim))
      return -1;
  }

  getrlimit(RLIMIT_NPROC, &lim);                     // get file des limit
  if (lim.rlim_cur < (rlim_t)(used_lim + clients)) { // set file des limit
    lim.rlim_cur = used_lim + clients;
    lim.rlim_max = used_lim + clients;
    if (setrlimit(RLIMIT_NPROC, &lim))
      return -2;
  }

  return 0;
}
/* 解析十进制整数；成功返回 0，非法、负数或溢出返回 -1 */
static int parse_int(const char *str, int *value) {
  char *end = NULL;
  long parsed;

  if (str == NULL || *str == '\0')
    return -1;
  errno = 0;
  parsed = strtol(str, &end, 10);
  if (errno != 0 || end == str || *end != '\0' || parsed < 0 ||
      parsed > INT_MAX)
    return -1;
  *value = (int)parsed;
  return 0;
}

static void usage(void) {
  fprintf(
      stderr,
      "superwebbench [option]... URL\n"
      "  -f|--force                      Don't wait for reply from server.\n"
      "  -r|--reload                     Send reload request - Pragma: "
      "no-cache.\n"
      "  -t|--time <sec>                 Run benchmark for <sec> seconds. "
      "Default 30.\n"
      "  -p|--proxy <server:port>        Use proxy server for request.\n"
      "  -c|--clients <n>                Run <n> HTTP clients at once. Default "
      "one.\n"
      "  -o|--output <text|json>         Select result output format. Default "
      "text.\n"
      "  -9|--http09                     Use HTTP/0.9 style requests.\n"
      "  -1|--http10                     Use HTTP/1.0 protocol.\n"
      "  -2|--http11                     Use HTTP/1.1 protocol.\n"
      "  --get                           Use GET request method.\n"
      "  --head                          Use HEAD request method.\n"
      "  --options                       Use OPTIONS request method.\n"
      "  --trace                         Use TRACE request method.\n"
      "  -?|-h|--help                    This information.\n"
      "  -V|--version                    Display program version.\n");
};
static int parse_opt(int argc, char *argv[], struct parameter *para) {
  const struct option long_options[] = {
      {"force", no_argument, &para->force, 1},
      {"reload", no_argument, &para->force_reload, 1},
      {"get", no_argument, &para->method, METHOD_GET},
      {"head", no_argument, &para->method, METHOD_HEAD},
      {"options", no_argument, &para->method, METHOD_OPTIONS},
      {"trace", no_argument, &para->method, METHOD_TRACE},
      {"help", no_argument, NULL, '?'},
      {"http09", no_argument, NULL, '9'},
      {"http10", no_argument, NULL, '1'},
      {"http11", no_argument, NULL, '2'},
      {"version", no_argument, NULL, 'V'},
      {"time", required_argument, NULL, 't'},
      {"proxy", required_argument, NULL, 'p'},
      {"clients", required_argument, NULL, 'c'},
      {"output", required_argument, NULL, 'o'},
      {NULL, 0, NULL, 0}};

  int opt = 0;
  int options_index = 0;
  char *tmp = NULL;

  if (argc == 1) {
    usage();
    return 0;
  }

  while ((opt = getopt_long(argc, argv, "912Vfrt:p:c:o:?h", long_options,
                            &options_index)) != EOF) {
    switch (opt) {
    case 0:
      break;
    case 'f':
      para->force = 1;
      break;
    case 'r':
      para->force_reload = 1;
      break;
    case '9':
      para->http_ver = HTTP_09;
      break;
    case '1':
      para->http_ver = HTTP_10;
      break;
    case '2':
      para->http_ver = HTTP_11;
      break;
    case 'V':
      printf(PROGRAM_VERSION "\n");
      return 0;
    case 't':
      if (parse_int(optarg, &para->bench_time) != 0 || para->bench_time <= 0) {
        fprintf(stderr, "Error in option --time %s: invalid seconds.\n", optarg);
        return -1;
      }
      break;
    case 'p':
      para->proxy = 1;
      /* proxy server parsing server:port */
      tmp = strrchr(optarg, ':');
      if (tmp == NULL) {
        fprintf(stderr, "Error in option --proxy %s: Port number is missing.\n",
                optarg);
        return -1;
      }
      if (tmp == optarg) {
        fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n",
                optarg);
        return -1;
      }
      if (tmp == optarg + strlen(optarg) - 1) {
        fprintf(stderr, "Error in option --proxy %s Port number is missing.\n",
                optarg);
        return -1;
      }
      *tmp = '\0';
      /* 保存代理主机名，供后续建立连接使用 */
      if (strlen(optarg) >= MAXHOSTNAMELEN) {
        fprintf(stderr, "Error in option --proxy %s: hostname too long.\n",
                optarg);
        return -1;
      }
      strcpy(para->host, optarg);
      if (parse_int(tmp + 1, &para->port) != 0 || para->port <= 0 ||
          para->port > 65535) {
        fprintf(stderr, "Error in option --proxy %s Port number is invalid.\n",
                optarg);
        return -1;
      }
      break;
    case ':':
    case 'h':
    case '?':
      usage();
      return 0;
      break;
    case 'c':
      if (parse_int(optarg, &para->clients) != 0 || para->clients <= 0) {
        fprintf(stderr, "Error in option --clients %s: invalid client count.\n",
                optarg);
        return -1;
      }
      break;
    case 'o':
      if (strcmp(optarg, "text") == 0) {
        para->output = OUTPUT_TEXT;
      } else if (strcmp(optarg, "json") == 0) {
        para->output = OUTPUT_JSON;
      } else {
        fprintf(stderr, "Error in option --output %s: expected text or json.\n",
                optarg);
        return -1;
      }
      break;
    }
  }

  if (optind == argc) {
    fprintf(stderr, "superwebbench: Missing URL!\n");
    usage();
    return 2;
  }

  return 1;
}

int main(int argc, char *argv[]) {
  int ret;
  char host[MAXHOSTNAMELEN];
  char request[REQUEST_SIZE];
  struct parameter para = {
      DEFAULT_FORCE,    DEFAULT_FORE_RELOAD, DEFAULT_PROXY,
      DEFAULT_HTTPS,    DEFAULT_HTTP_VER,    DEFAULT_METHOD,
      DEFAULT_HTTP_PORT, DEFAULT_CLIENT,     DEFAULT_TIME,
      DEFAULT_OUTPUT,   host,                request,
  };
  // parse options
  if ((ret = parse_opt(argc, argv, &para)) <= 0)
    return ret < 0 ? 2 : 0;

  build_request(argv[argc - 1], &para);
  if (para.output == OUTPUT_TEXT) {
    /* Copyright */
    printf("\nSuperWebBench - Advanced Simple Web Benchmark " PROGRAM_VERSION "\n"
           "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
           "Modified By Davelv 2011-11-03\n");
  }
  // check resourse limits
  if ((ret = resource_set(para.clients)) < 0) {
    fprintf(stderr,
            "\nSet %s  limit failed. \n"
            "Try less clients or use higher authority or set \"ulimit -n -u\" "
            "manualy\n",
            ret == -1 ? "NOFILE" : "NPROC");
    return 4;
  }
  /* print bench info */
  if (para.output == OUTPUT_TEXT) {
    printf("\nBenchmarking:%s %s (using %s)\n", gs_http_method[para.method],
           argv[argc - 1], gs_http_version[para.http_ver]);
  }
  /* check avaibility of target server */
  if (para.https && Connection_init() < 0) {
    fprintf(stderr, "\nInitialize TLS failed. Aborting benchmark.\n");
    Connection_print_errors();
    Connection_cleanup();
    return 3;
  }
  {
    Connection connection;
    if (Connection_open(&connection, para.host, para.port, para.https) < 0) {
      fprintf(stderr,
              "\nConnect to server or establish TLS session failed. Aborting "
              "benchmark.\n");
      if (para.https)
        Connection_print_errors();
      Connection_cleanup();
      return 1;
    }
    Connection_close(&connection);
  }
  ret = bench(&para, argv[argc - 1]);
  if (para.https)
    Connection_cleanup();
  return ret;
}

void build_request(const char *url, struct parameter *para) {
  const char *host_start;
  const char *path;
  const char *port_str = NULL;
  size_t host_len;
  size_t off = 0;
  char *req = para->request;
  int i;

/* 追加到请求缓冲区，长度不足时报错退出，避免缓冲区溢出 */
#define APPEND(...)                                                          \
  do {                                                                       \
    int written = snprintf(req + off, REQUEST_SIZE - off, __VA_ARGS__);      \
    if (written < 0 || (size_t)written >= REQUEST_SIZE - off) {              \
      fprintf(stderr, "Request is too long.\n");                             \
      exit(2);                                                               \
    }                                                                        \
    off += (size_t)written;                                                  \
  } while (0)

  if (!para->proxy)
    memset(para->host, 0, MAXHOSTNAMELEN);
  memset(para->request, 0, REQUEST_SIZE);

  if (para->force_reload && para->proxy && para->http_ver < HTTP_10)
    para->http_ver = HTTP_10;
  if (para->method == METHOD_HEAD && para->http_ver < HTTP_10)
    para->http_ver = HTTP_10;
  if (para->method == METHOD_OPTIONS && para->http_ver < HTTP_11)
    para->http_ver = HTTP_11;
  if (para->method == METHOD_TRACE && para->http_ver < HTTP_11)
    para->http_ver = HTTP_11;

  if (NULL == strstr(url, "://")) {
    fprintf(stderr, "\n%s: is not a valid URL.\n", url);
    exit(2);
  }
  if (strlen(url) > URL_SIZE) {
    fprintf(stderr, "URL is too long.\n");
    exit(2);
  }
  if (!para->proxy) {
    if (0 == strncasecmp("http://", url, 7)) {
      para->https = 0;
      para->port = DEFAULT_HTTP_PORT;
    } else if (0 == strncasecmp("https://", url, 8)) {
      para->https = 1;
      para->port = DEFAULT_HTTPS_PORT;
    } else {
      fprintf(stderr, "\nOnly HTTP and HTTPS protocols are directly supported, "
                      "set --proxy for others.\n");
      exit(2);
    }
  }
  /* protocol/host delimiter */
  i = strstr(url, "://") - url + 3;

  path = strchr(url + i, '/');
  if (path == NULL) {
    fprintf(stderr, "\nInvalid URL syntax - hostname don't ends with '/'.\n");
    exit(2);
  }

  APPEND("%s ", gs_http_method[para->method]);

  if (para->proxy) {
    APPEND("%s", url);
  } else {
    host_start = url + i;
    if (*host_start == '[') {
      /* IPv6 字面量，如 http://[::1]/ 或 http://[::1]:8080/ */
      const char *bracket = strchr(host_start, ']');
      if (bracket == NULL || bracket > path) {
        fprintf(stderr, "\nInvalid IPv6 address in URL.\n");
        exit(2);
      }
      host_start++;
      host_len = (size_t)(bracket - host_start);
      if (bracket + 1 < path) {
        if (bracket[1] != ':') {
          fprintf(stderr, "\nInvalid URL syntax after IPv6 address.\n");
          exit(2);
        }
        port_str = bracket + 2;
      }
    } else {
      const char *colon = strchr(host_start, ':');
      if (colon != NULL && colon < path) {
        host_len = (size_t)(colon - host_start);
        port_str = colon + 1;
      } else {
        host_len = (size_t)(path - host_start);
      }
    }
    if (host_len == 0 || host_len >= MAXHOSTNAMELEN) {
      fprintf(stderr, "\nInvalid hostname in URL.\n");
      exit(2);
    }
    memcpy(para->host, host_start, host_len);
    para->host[host_len] = '\0';

    if (port_str != NULL) {
      size_t port_len = (size_t)(path - port_str);
      char port_buf[16];
      if (port_len == 0 || port_len >= sizeof port_buf) {
        fprintf(stderr, "\nInvalid port in URL.\n");
        exit(2);
      }
      memcpy(port_buf, port_str, port_len);
      port_buf[port_len] = '\0';
      if (parse_int(port_buf, &para->port) != 0 || para->port <= 0 ||
          para->port > 65535) {
        fprintf(stderr, "\nInvalid port in URL.\n");
        exit(2);
      }
    }
    APPEND("%s", path);
  }

  if (para->http_ver == HTTP_10)
    APPEND(" HTTP/1.0");
  else if (para->http_ver == HTTP_11)
    APPEND(" HTTP/1.1");
  APPEND("\r\n");
  if (para->http_ver > 0)
    APPEND("User-Agent: SuperWebBench " PROGRAM_VERSION "\r\n");
  if (!para->proxy && para->http_ver > 0)
    APPEND("Host: %s\r\n", para->host);
  if (para->force_reload && para->proxy)
    APPEND("Pragma: no-cache\r\n");
  if (para->http_ver > 1)
    APPEND("Connection: close\r\n");
  /* add empty line at end */
  if (para->http_ver > 0)
    APPEND("\r\n");

#undef APPEND
}

/* vraci system rc error kod */
static int bench(struct parameter *para, const char *url) {
  int i;
  unsigned long long speed = 0;
  unsigned long long failed = 0;
  unsigned long long httperr = 0;
  unsigned long long bytes = 0;
  pthread_attr_t attr;
  // set thread args
  struct thread_arg *args =
      calloc((size_t)para->clients, sizeof(struct thread_arg));
  if (args == NULL) {
    fprintf(stderr, "Malloc thread args failed. Aborting benchmark.\n");
    return 3;
  }
  for (i = 0; i < para->clients; i++) {
    args[i].host = para->host;
    args[i].port = para->port;
    args[i].request = para->request;
    args[i].http_ver = para->http_ver;
    args[i].force = para->force;
    args[i].https = para->https;
  }
  // theads
  pthread_attr_init(&attr);
  if (pthread_attr_setstacksize(&attr, DEFAULT_STACK_SIZE)) {
    fprintf(stderr, "Set stack size failed . Aborting benchmark.\n");
    free(args);
    return 3;
  }
  if (para->output == OUTPUT_TEXT) {
    printf("\n%d clients, running %d sec", para->clients, para->bench_time);
    if (para->force)
      printf(", early socket close");
    if (para->proxy)
      printf(", via proxy server %s:%d", para->host, para->port);
    if (para->force_reload)
      printf(", forcing reload");
    printf(".\n");
  }
  // new thread
  atomic_store(&bench_stop, 0);
  for (i = 0; i < para->clients; i++) {
    int ret_p =
        pthread_create(&args[i].thread, &attr, bench_thread, args + i);
    if (ret_p) {
      fprintf(stderr, "pthread create error %d on %d\n", ret_p, i);
      /* 创建失败的槽位保持 started=0，计数为零，不影响其他客户端 */
    } else {
      args[i].started = 1;
    }
  }
  pthread_attr_destroy(&attr);
  // main thread sleep
  sleep(para->bench_time);
  /* 通知工作线程停止并等待其退出，确保计数完整、线程资源被回收 */
  atomic_store(&bench_stop, 1);
  for (i = 0; i < para->clients; i++) {
    if (args[i].started)
      pthread_join(args[i].thread, NULL);
  }
  // calc the result
  for (i = 0; i < para->clients; i++) {
    speed += args[i].speed;
    httperr += args[i].httperr;
    failed += args[i].failed;
    bytes += args[i].bytes;
  }
  free(args);
  print_result(para, url, speed, failed, httperr, bytes);
  return 0;
}

/* 输出 JSON 字符串，转义引号、反斜杠和控制字符，保证输出可解析 */
static void json_string(const char *value) {
  const unsigned char *p = (const unsigned char *)value;

  putchar('"');
  while (*p != '\0') {
    switch (*p) {
    case '"':
      fputs("\\\"", stdout);
      break;
    case '\\':
      fputs("\\\\", stdout);
      break;
    case '\b':
      fputs("\\b", stdout);
      break;
    case '\f':
      fputs("\\f", stdout);
      break;
    case '\n':
      fputs("\\n", stdout);
      break;
    case '\r':
      fputs("\\r", stdout);
      break;
    case '\t':
      fputs("\\t", stdout);
      break;
    default:
      if (*p < 0x20)
        printf("\\u%04x", *p);
      else
        putchar(*p);
      break;
    }
    p++;
  }
  putchar('"');
}

static void print_result(const struct parameter *para, const char *url,
                         unsigned long long speed,
                         unsigned long long failed,
                         unsigned long long httperr,
                         unsigned long long bytes) {
  unsigned long long total = speed + failed + httperr;

  if (para->output == OUTPUT_JSON) {
    printf("{\"version\":");
    json_string(PROGRAM_VERSION);
    printf(",\"url\":");
    json_string(url);
    printf(",\"host\":");
    json_string(para->host);
    printf(",\"port\":%d,\"method\":", para->port);
    json_string(gs_http_method[para->method]);
    printf(",\"http_version\":");
    json_string(gs_http_version[para->http_ver]);
    printf(",\"clients\":%d,\"duration_seconds\":%d,\"force\":%s,"
           "\"proxy\":%s,\"reload\":%s,\"total_requests\":%llu,"
           "\"successful_requests\":%llu,\"http_errors\":%llu,"
           "\"failed_requests\":%llu,\"bytes_received\":%llu,"
           "\"requests_per_second\":%llu,\"successful_requests_per_second\":%llu,"
           "\"bytes_per_second\":%llu}\n",
           para->clients, para->bench_time, para->force ? "true" : "false",
           para->proxy ? "true" : "false",
           para->force_reload ? "true" : "false", total, speed, httperr,
           failed, bytes, total / para->bench_time,
           speed / para->bench_time, bytes / para->bench_time);
  } else {
    printf("\nSpeed=%llu requests/sec (total), %llu pages/sec (success), %llu "
           "bytes/sec.\nRequests: %llu ok, %llu http error, %llu failed.\n",
           total / para->bench_time, speed / para->bench_time,
           bytes / para->bench_time, speed, httperr, failed);
  }
}

#define ERR_PROCESS(a, b, c)                                                   \
  {                                                                            \
    a++;                                                                       \
    Connection_close(&(b));                                                    \
    goto c;                                                                    \
  }
void *bench_thread(void *arg) {
  usleep(START_SLEEP);
  struct thread_arg *p_arg = (struct thread_arg *)arg;
  int rlen = strlen(p_arg->request);
  char buf[BUF_SIZE];
  int i, cnt;

  while (1) {
    Connection connection;
  outloop:
    if (atomic_load(&bench_stop))
      break;
    if (Connection_open(&connection, p_arg->host, p_arg->port, p_arg->https) <
        0) {
      p_arg->failed++;
      continue;
    }
    if (rlen != Connection_write(&connection, p_arg->request, rlen)) {
      ERR_PROCESS(p_arg->failed, connection, outloop);
    }
    /* TLS 没有半关闭写方向的操作；HTTP/0.9 请求本身也没有请求体。 */
    if (!p_arg->https && p_arg->http_ver == HTTP_09)
      if (shutdown(connection.fd, 1)) {
        ERR_PROCESS(p_arg->failed, connection, outloop);
      }
    if (p_arg->force == 0) {
      /* read all available data from socket */
      cnt = 0;
      while (1) {
        i = Connection_read(&connection, buf, BUF_SIZE);
        if (i < 0) {
          ERR_PROCESS(p_arg->failed, connection, outloop);
        } else if (i == 0) {
          if (cnt == 0) { // first read
            ERR_PROCESS(p_arg->httperr, connection, outloop);
          }
          break;
        } else { // check http status
          p_arg->bytes += i;
          if ((cnt++ == 0) && (http_response_check(buf) < 0)) {
            ERR_PROCESS(p_arg->httperr, connection, outloop);
          }
        }
      }
    }
    if (Connection_close(&connection)) {
      p_arg->failed++;
      continue;
    }
    p_arg->speed++;
  }
  return NULL;
}
int http_response_check(const char *response) {
  int status = 0;
  float version;
  // HTTP/
  response += 5;
  if ((sscanf(response, "%f %d", &version, &status) == 2) &&
      (status >= 200 && status < 300))
    return 0;

  else
    return -1;
}
