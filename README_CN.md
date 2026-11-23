# SuperWebBench

**简体中文** | [English](README.md)

[![GitHub stars](https://img.shields.io/github/stars/xutongle/superwebbench?style=flat-square)](https://github.com/xutongle/superwebbench/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/xutongle/superwebbench?style=flat-square)](https://github.com/xutongle/superwebbench/network/members)
[![GitHub issues](https://img.shields.io/github/issues/xutongle/superwebbench?style=flat-square)](https://github.com/xutongle/superwebbench/issues)
[![GitHub license](https://img.shields.io/github/license/xutongle/superwebbench?style=flat-square)](LICENSE)

基于 Webbench 重构与扩展的命令行压测工具，使用 C11 和 POSIX Threads 模拟多客户端并发请求，支持 HTTP/HTTPS、IPv4/IPv6，能够统计请求成功率、HTTP 错误、失败请求、QPS 及网络吞吐量，用于 Web 服务性能测试与容量评估。

当前版本：**1.0.1**，详细变更见 [Changelog](Changelog)。

> 请仅对自己拥有或获得明确授权的服务器进行压测。高并发请求可能消耗大量网络、连接和系统资源，并影响目标服务及同机其他业务。

## 功能特性

- 使用 POSIX 线程模拟并发客户端，默认线程栈大小为 32 KiB。
- 到时后工作线程正常停止，主线程 `pthread_join` 回收并完整汇总计数。
- 支持 IPv4 与 IPv6 目标，地址解析使用线程安全的 `getaddrinfo`。
- 支持直接测试 HTTPS 地址，使用 OpenSSL 建立 TLS 连接，发送 SNI 并验证证书链及主机名或 IP。
- 支持 GET、HEAD、OPTIONS、TRACE 请求方法选项。
- 支持通过 `-H` / `--header` 重复添加自定义 HTTP 请求头。
- 支持 HTTP/0.9、HTTP/1.0 和 HTTP/1.1。
- 支持自定义并发客户端数和测试时长。
- 支持发送后不等待响应的 `--force` 模式。
- 提供代理和禁止代理缓存相关选项。
- 普通模式下检查 HTTP 响应状态，仅将 2xx 响应计为成功。
- 统计成功请求、HTTP 错误、失败请求、每秒请求数和每秒接收字节数。
- 支持人类可读的文本输出和便于脚本解析的 JSON 输出。

## 运行环境

项目主要面向 Linux 和其他 Unix-like 系统，使用以下系统接口和工具：

- 支持 C11 `<stdatomic.h>` 的 C 编译器、POSIX Shell 和 Make。
- POSIX Threads（pthread）。
- POSIX/BSD Socket、`getaddrinfo`（IPv4/IPv6）和 `getopt_long`。
- OpenSSL 1.1 或更高版本（`libssl` 和 `libcrypto`），并提供 TLS 客户端、SNI、主机名和 IP 校验 API。
- `sys/resource.h` 中的资源限制接口；没有 `RLIMIT_NPROC` 的平台仅跳过该项限制调整。

源码未针对 Windows 进行适配。CI 已覆盖 Ubuntu、macOS、CentOS Stream 9 和 Alpine；其他 Unix-like 平台通过编译期的能力探测进行适配，而不是按操作系统名称判断。

`configure` 优先通过 `pkg-config` 获取 OpenSSL 编译和链接参数，不可用时回退到 `-lssl -lcrypto`。如果 OpenSSL 安装在非标准路径，可以通过 `OPENSSL_CFLAGS` 和 `OPENSSL_LIBS` 显式指定。

`ctags` 仅用于生成源码索引，不是编译和运行的必需依赖。Autoconf 和 Automake 提供的 `aclocal` 仅在重新生成 `configure` 时需要；普通构建直接使用仓库和源码包中已生成的文件。

CentOS 7 需要支持 C11 原子操作的新编译器和 OpenSSL 1.1 或更高版本。原生 GCC 4.8、OpenSSL 1.0.2 工具链会在配置阶段被明确拒绝，不会为了兼容而削弱证书校验。

## 编译与安装

### 编译

在项目根目录执行：

```bash
./configure
make
```

编译完成后生成 `superwebbench` 可执行文件。可以在配置阶段指定其他安装前缀：

```bash
./configure --prefix=/usr/local
```

OpenSSL 安装在非标准路径时：

```bash
./configure OPENSSL_CFLAGS="-I/opt/openssl/include" \
  OPENSSL_LIBS="-L/opt/openssl/lib -lssl -lcrypto"
```

在 macOS 上使用 Homebrew OpenSSL 时，如有必要，可在配置前设置 `PKG_CONFIG_PATH`：

```bash
PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig" ./configure
```

如果不需要生成 ctags 索引，可以只编译程序：

```bash
make superwebbench
```

清理编译产物，或同时删除 `configure` 生成的配置文件：

```bash
make clean
make distclean
```

也支持在源码目录外构建：

```bash
mkdir build
cd build
../configure
make
```

维护者修改 `configure.ac` 后，可以重新生成仓库中的构建文件，再执行配置：

```bash
autoreconf -fi
```

### 安装

默认安装到 `/usr` 目录下：

```bash
sudo make install
```

也可以指定其他安装前缀，例如：

```bash
sudo make install PREFIX=/usr/local
```

安装内容包括：

- 可执行文件：`$(PREFIX)/bin/superwebbench`
- man 手册：`$(PREFIX)/share/man/man1/superwebbench.1`
- 项目文档：`$(PREFIX)/share/doc/superwebbench/`

安装后可查看命令手册：

```bash
man superwebbench
```

构建软件包时，可以通过 `DESTDIR` 安装到暂存目录：

```bash
make install DESTDIR=/tmp/superwebbench-package PREFIX=/usr
```

### 源码打包

以下命令会在不清理当前工作区的情况下生成 `superwebbench-1.0.1.tar.gz`：

```bash
make tar
```

源码包包含 `configure`、`config.h.in` 和 `install-sh`，接收者无需安装 Autoconf 或 Automake，直接执行 `./configure && make` 即可构建。

### 自动化构建与发布

GitHub Actions 会在 Ubuntu、macOS、CentOS Stream 9 和 Alpine 上编译，执行冒烟测试并验证暂存安装。Ubuntu 还会验证维护者再生成流程、本地 HTTP 测试以及从源码包重新构建。推送任意标签时会创建 GitHub Release，上传经过重建验证的源码包和 `superwebbench-<tag>-linux-x86_64.tar.gz` 二进制包。

## 快速开始

查看帮助和版本：

```bash
./superwebbench --help
./superwebbench --version
```

使用默认配置测试目标地址，即 1 个客户端运行 30 秒：

```bash
./superwebbench http://127.0.0.1/
```

使用 100 个并发客户端测试 30 秒：

```bash
./superwebbench -c 100 -t 30 http://example.com/
```

使用 HEAD 方法、HTTP/1.0 协议，以 50 个客户端测试 10 秒：

```bash
./superwebbench -c 50 -t 10 --head -1 http://example.com/
```

测试非默认端口：

```bash
./superwebbench -c 20 -t 10 http://127.0.0.1:8080/
```

直接测试 HTTPS 地址，未指定端口时默认使用 443：

```bash
./superwebbench -c 20 -t 10 https://example.com/
./superwebbench -c 20 -t 10 https://example.com:8443/
```

HTTPS 默认验证 CA 证书链以及目标主机名或 IP。使用私有 CA 时，可通过 OpenSSL 标准的 `SSL_CERT_FILE` 或 `SSL_CERT_DIR` 环境变量配置信任证书。

通过重复的 `-H` 或 `--header` 参数添加自定义请求头：

```bash
./superwebbench \
  -H "Authorization: Bearer your-token" \
  -H "Content-Type: application/json" \
  -H "X-Request-Source: benchmark" \
  -c 20 -t 30 \
  https://example.com/api
```

每个参数添加一个 `Name: Value` 格式的请求头，自定义请求头按参数顺序追加到默认请求头之后，同名字段不会去重。字段名必须符合 HTTP token 规则，值不能包含换行等控制字符。HTTP/0.9 不支持请求头，因此同时使用 `--http09` 和 `-H` 时会自动升级到 HTTP/1.0。

发送请求后不读取响应，直接关闭连接：

```bash
./superwebbench -c 20 -t 10 --force http://127.0.0.1/
```

`--force` 模式会产生更高的请求压力，其成功计数只表示请求发送和连接关闭流程完成，不代表服务器返回了 HTTP 2xx 响应。

以 JSON 格式输出结果，便于脚本或 CI 解析：

```bash
./superwebbench -c 20 -t 10 --output json http://127.0.0.1:8080/
```

也可以将结果直接保存到文件：

```bash
./superwebbench -c 20 -t 10 -o json http://127.0.0.1:8080/ > result.json
```

## 命令行用法

```text
superwebbench [options] URL
```

| 选项 | 说明 |
| --- | --- |
| `-c`, `--clients <n>` | 并发客户端数，默认为 `1`；非数字、负数或 0 会报错并以状态码 2 退出 |
| `-t`, `--time <sec>` | 测试时长（秒），默认为 `30`；非数字、负数或 0 会报错并以状态码 2 退出 |
| `-f`, `--force` | 发送请求后不等待响应，直接关闭连接 |
| `-r`, `--reload` | 使用代理时发送 `Pragma: no-cache` 请求头；代理与 HTTP/0.9 同用时会自动升级到 HTTP/1.0 |
| `-p`, `--proxy <server:port>` | 通过指定代理服务器 `server:port` 发送请求 |
| `-o`, `--output <format>` | 结果格式：`text`（默认）或 `json`；其他值会报错并以状态码 2 退出 |
| `-H`, `--header <name:value>` | 添加自定义 HTTP 请求头，可重复使用；格式错误或包含控制字符时以状态码 2 退出 |
| `-9`, `--http09` | 使用 HTTP/0.9 风格请求；指定自定义请求头时自动升级到 HTTP/1.0 |
| `-1`, `--http10` | 使用 HTTP/1.0 |
| `-2`, `--http11` | 使用 HTTP/1.1，默认值；不支持 Keep-Alive |
| `--get` | 使用 GET 方法，默认值 |
| `--head` | 使用 HEAD 方法；HTTP/0.9 下会自动升级到 HTTP/1.0 |
| `--options` | 使用 OPTIONS 方法；HTTP/1.0 及以下会自动升级到 HTTP/1.1 |
| `--trace` | 使用 TRACE 方法；HTTP/1.0 及以下会自动升级到 HTTP/1.1 |
| `-?`, `-h`, `--help` | 显示帮助信息 |
| `-V`, `--version` | 显示程序版本 |

注意：版本选项的短参数是大写 `-V`，不是小写 `-v`。

## URL 规则

直接请求目标服务器时：

- 支持 `http://` 和 `https://`，默认端口分别为 80 和 443。
- 主机名后必须包含路径，根路径也需要显式写成 `/`。
- 支持在主机名后指定端口，端口须为 1–65535 的数字。
- 支持 IPv4、主机名（可解析到 AAAA 记录）以及 `[::1]` 形式的 IPv6 字面量。
- URL 最大长度为 1500 字节。

正确示例：

```text
http://example.com/
https://example.com/
https://example.com:8443/api?id=1
http://example.com/index.html
http://example.com:8080/api?id=1
http://127.0.0.1:8080/
https://[::1]:8443/
```

以下地址会被判定为无效：

```text
example.com
ftp://example.com/
http://example.com
https://example.com
```

其中 `http://example.com` 和 `https://example.com` 缺少主机名后的 `/`，应分别在末尾补上 `/`。

## 输出说明

测试开始前，程序会输出目标 URL、请求方法、HTTP 版本、客户端数和测试时长。测试结束后输出汇总结果：

```text
Speed=3008 requests/sec (total), 3008 pages/sec (success), 2972398 bytes/sec.
Requests: 6017 ok, 0 http error, 0 failed.
```

以上数值仅为输出格式示例，各字段含义如下：

| 字段 | 含义 |
| --- | --- |
| `requests/sec (total)` | 平均每秒总请求数，计算时包含成功、HTTP 错误和失败请求，反映整体吞吐 |
| `pages/sec (success)` | 平均每秒成功请求数，仅统计 HTTP 2xx 响应 |
| `bytes/sec` | 平均每秒从服务器接收的响应字节数 |
| `ok` | 成功请求数；普通模式下仅统计 HTTP 2xx 响应 |
| `http error` | HTTP 响应未通过 2xx 检查，或未读取到有效响应等异常数量 |
| `failed` | 连接、写入、读取或关闭 Socket 等操作失败的请求数 |

每个请求都会重新建立 TCP 连接，HTTPS 请求还会重新完成 TLS 握手。普通模式读取响应直到服务器关闭连接，HTTP/1.1 请求会携带 `Connection: close`。

### JSON 输出

使用 `-o json` 或 `--output json` 时，标准输出仅包含一个完整的 JSON 对象，不再输出版权和进度文本。错误信息仍写入标准错误，退出状态码规则不变。

```json
{"version":"1.0.1","url":"http://127.0.0.1:8080/","host":"127.0.0.1","port":8080,"method":"GET","http_version":"HTTP/1.1","clients":20,"duration_seconds":10,"force":false,"proxy":false,"reload":false,"total_requests":6017,"successful_requests":6017,"http_errors":0,"failed_requests":0,"bytes_received":29723980,"requests_per_second":601,"successful_requests_per_second":601,"bytes_per_second":2972398}
```

字段对应关系：

| JSON 字段 | 含义 |
| --- | --- |
| `total_requests` | 总请求数，等于成功、HTTP 错误和失败请求数之和 |
| `successful_requests` | 成功请求数 |
| `http_errors` | HTTP 错误请求数 |
| `failed_requests` | 失败请求数 |
| `bytes_received` | 接收的响应总字节数 |
| `requests_per_second` | 平均每秒总请求数 |
| `successful_requests_per_second` | 平均每秒成功请求数 |
| `bytes_per_second` | 平均每秒接收字节数 |

其余字段记录压测配置，包括 URL、主机、端口、方法、HTTP 版本、客户端数、时长及 force/proxy/reload 开关。JSON 字符串会转义引号、反斜杠和控制字符。

## 退出状态码

| 状态码 | 含义 |
| --- | --- |
| `0` | 正常结束，或显示了帮助、版本信息；未知选项或选项缺少必填参数时也会显示帮助并返回此状态码 |
| `1` | 压测前连接目标失败，或 HTTPS 的 TLS 握手、证书校验失败 |
| `2` | 命令行参数值或 URL 无效 |
| `3` | 内部错误，例如内存分配、线程属性设置或 TLS 初始化失败 |
| `4` | 设置系统资源限制失败 |

## 常见问题

### 未安装 `ctags`

默认 `make` 会提示 `ctags not found, skipping tags generation` 并自动跳过源码索引生成，不影响程序编译。需要生成索引时可以安装 ctags，也可以只编译程序：

```bash
make superwebbench
```

### 提示 `Connect to server failed`

请检查：

- URL 是否符合上述格式。
- 目标服务是否已启动，端口是否正确。
- 本机能否解析目标主机名并建立 TCP 连接。
- HTTPS 证书是否由受信任的 CA 签发，且证书中的主机名或 IP 与 URL 匹配。
- 防火墙或网络策略是否阻止连接。

### 提示 `Set NOFILE/NPROC limit failed`

程序会根据客户端数尝试调整文件描述符和进程数限制。如果当前用户权限或系统硬限制不足，程序会退出。请降低 `--clients`，或根据操作系统要求调整 `ulimit -n`、`ulimit -u` 及相应系统限制。

## 工作原理

1. 解析命令行参数、目标主机、端口和请求路径。
2. 构造 HTTP 请求行及 `User-Agent`、`Host`、`Connection` 等默认请求头，并追加自定义请求头。
3. 根据并发客户端数检查并调整系统资源限制。
4. 建立一次测试连接，提前确认目标是否可访问；HTTPS 还会完成 TLS 握手和证书校验。
5. 为每个客户端创建一个 POSIX 工作线程。
6. 工作线程循环执行“连接、发送请求、读取响应、关闭连接、更新计数”，HTTPS 连接在发送请求前建立 TLS 会话。
7. 到达指定时长后，主线程置位停止标志，等待各工作线程结束当前请求并退出（`pthread_join`），再汇总计数输出结果。若目标服务器长时间不响应，退出会等待进行中的请求完成。

## 当前限制与已知问题

- `--proxy` 按原方式发送 HTTP 代理请求，不支持通过 `CONNECT` 建立 HTTPS 隧道；直接测试 HTTPS 时不要指定该选项。
- HTTP/1.1 不支持 Keep-Alive，每次请求都会重新建立连接。
- 请求行与全部请求头共用 2048 字节内部缓冲区，请求过长时会报错并以状态码 2 退出。
- HTTP/0.9 响应没有状态行，普通模式下无法通过 2xx 状态检查，通常会计入 `http error`；`--force` 模式不读取和校验响应。

## 项目结构

```text
.
├── .github/workflows/
│   ├── ci.yml         # Linux、macOS、CentOS Stream 和 Alpine 构建
│   └── release.yml    # 标签触发的 GitHub Release 及源码包重建验证
├── configure.ac      # Autoconf 能力探测和包版本定义
├── configure         # 生成的配置脚本
├── config.h.in       # 生成的配置头模板
├── install-sh        # 可移植安装辅助脚本
├── Makefile.in       # 编译、安装、清理和打包模板
├── superwebbench.c   # 主程序、参数解析、HTTP 请求构造和压测逻辑
├── socket.c          # IPv4/IPv6 TCP 连接及 OpenSSL TLS 传输
├── socket.h          # Socket 与 TLS 连接接口声明
├── superwebbench.1   # man 手册
├── README            # 原始项目说明
├── README.md         # 英文使用说明
├── README_CN.md      # 中文使用说明
├── Changelog         # 历史变更记录
├── LICENSE           # GNU GPL v2 许可证全文
├── .gitattributes    # Git 导出属性
└── .gitignore        # Git 忽略规则
```

## 版权与许可证

- 原 Webbench 作者：Radim Kolar，1997–2004。
- SuperWebBench 修改作者：XuTongle，2012。
- `socket.c` 中的 UNIX Socket 代码取自 popclient 1.5，原始代码为 public domain，由 Virginia Tech Computing Center 创建。
- 主程序源码声明采用 **GNU General Public License version 2（GPL v2）**。

原始版权说明保留在源码、`README` 和 `superwebbench.1` 中，许可证全文见 [LICENSE](LICENSE)。
