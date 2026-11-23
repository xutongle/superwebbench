/*
 * tls.h - TLS 后端抽象层
 *
 * superwebbench 优先使用 OpenSSL，在 OpenSSL 不可用时回退到 mbed TLS。
 * 本头文件定义与具体 TLS 库无关的统一接口，socket.c 只依赖此接口，
 * 从而把 OpenSSL / mbed TLS 的差异隔离在各自的实现文件里。
 *
 * 接口约定（阻塞模型）：
 *   - TLS_read / TLS_write 返回 >0 表示成功传输的字节数，
 *     返回 0 表示对端干净关闭（EOF），返回 -1 表示出错。
 *   - 中断与重入（EINTR / WANT_READ / WANT_WRITE）由实现内部重试，
 *     调用者无需关心。
 *   - 所有句柄在出错时打印错误信息到 stderr（见 TLS_print_errors）。
 */
#ifndef _WEBBENCH_TLS_H
#define _WEBBENCH_TLS_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明类型，具体结构由所选后端定义。 */
typedef struct tls_ctx_st TLS_CTX;
typedef struct tls_st TLS;

/*
 * 创建并初始化进程级 TLS 上下文。
 * 内部加载系统 CA 根证书并启用证书链校验。
 * 成功返回上下文指针，失败返回 NULL（同时向 stderr 输出诊断信息）。
 */
TLS_CTX *TLS_CTX_create(void);

/* 释放由 TLS_CTX_create 创建的上下文。传入 NULL 安全。 */
void TLS_CTX_destroy(TLS_CTX *ctx);

/*
 * 基于已连接的文件描述符 fd 创建一个 TLS 连接对象。
 * 仅创建对象，不发起握手。失败返回 NULL。
 */
TLS *TLS_create(TLS_CTX *ctx, int fd);

/* 释放 TLS 连接对象。传入 NULL 安全，不会关闭底层 fd。 */
void TLS_destroy(TLS *tls);

/*
 * 为连接配置 SNI 与证书主机名/IP 校验。
 * host 可以是 DNS 主机名，也可以是 IPv4/IPv6 字面量。
 * 成功返回 0，失败返回 -1。
 */
int TLS_set_peer_name(TLS *tls, const char *host);

/*
 * 执行 TLS 握手，阻塞直到完成。成功返回 0，失败返回 -1。
 */
int TLS_handshake(TLS *tls);

/* 阻塞式读/写，语义见文件顶部约定。 */
ssize_t TLS_read(TLS *tls, void *buf, size_t size);
ssize_t TLS_write(TLS *tls, const void *buf, size_t size);

/* 发送 TLS close_notify 关闭告警。成功返回 0，失败返回 -1。 */
int TLS_shutdown(TLS *tls);

/* 把当前线程累积的 TLS 错误栈打印到 stderr。 */
void TLS_print_errors(void);

/* 返回编译时选中的后端名称，例如 "OpenSSL" 或 "mbed TLS"。 */
const char *TLS_backend_name(void);

#ifdef __cplusplus
}
#endif

#endif /* _WEBBENCH_TLS_H */
