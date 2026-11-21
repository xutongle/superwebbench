# SuperWebBench

SuperWebBench 是一个轻量级命令行 HTTP 基准测试工具，由 Webbench 修改而来。它使用 POSIX 线程模拟多个并发客户端，通过反复建立 TCP 连接、发送 HTTP 请求并读取响应，统计请求数量、错误数量和吞吐量。

当前版本：**0.1**。

> 请仅对自己拥有或获得明确授权的服务器进行压测。高并发请求可能消耗大量网络、连接和系统资源，并影响目标服务及同机其他业务。

## 功能特性

- 使用 POSIX 线程模拟并发客户端，默认线程栈大小为 32 KiB。
- 支持 GET、HEAD、OPTIONS、TRACE 请求方法选项。
- 支持 HTTP/0.9、HTTP/1.0 和 HTTP/1.1。
- 支持自定义并发客户端数和测试时长。
- 支持发送后不等待响应的 `--force` 模式。
- 提供代理和禁止代理缓存相关选项。
- 普通模式下检查 HTTP 响应状态，仅将 2xx 响应计为成功。
- 统计成功请求、HTTP 错误、失败请求、每秒请求数和每秒接收字节数。

## 运行环境

项目主要面向 Linux 和其他 Unix-like 系统，使用以下系统接口和工具：

- C 编译器（默认使用 `gcc`）和 Make。
- POSIX Threads（pthread）。
- POSIX/BSD Socket、IPv4 DNS 解析和 `getopt_long`。
- `sys/resource.h` 中的资源限制接口。
- `rpc/types.h` 等系统头文件。

源码未针对 Windows 进行适配。macOS 或其他 Unix-like 平台可能需要根据系统头文件和资源限制接口进行适配验证。

`ctags` 仅用于生成源码索引，不是编译和运行的必需依赖。默认 `make` 会尝试执行 `ctags *.c`，未安装时会显示错误，但该错误会被忽略，不影响二进制文件生成。

## 编译与安装

### 编译

在项目根目录执行：

```bash
make
```

编译完成后生成 `superwebbench` 可执行文件。

如果不需要生成 ctags 索引，可以只编译程序：

```bash
make superwebbench
```

清理编译产物：

```bash
make clean
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

发送请求后不读取响应，直接关闭连接：

```bash
./superwebbench -c 20 -t 10 --force http://127.0.0.1/
```

`--force` 模式会产生更高的请求压力，其成功计数只表示请求发送和连接关闭流程完成，不代表服务器返回了 HTTP 2xx 响应。

## 命令行用法

```text
superwebbench [options] URL
```

| 选项 | 说明 |
| --- | --- |
| `-c`, `--clients <n>` | 并发客户端数，默认为 `1`；小于等于 0 时恢复默认值 |
| `-t`, `--time <sec>` | 测试时长，单位为秒，默认为 `30`；小于等于 0 时恢复默认值 |
| `-f`, `--force` | 发送请求后不等待响应，直接关闭连接 |
| `-r`, `--reload` | 使用代理时发送 `Pragma: no-cache` 请求头 |
| `-p`, `--proxy <server:port>` | 通过指定代理发送请求；当前实现存在已知问题，不建议使用 |
| `-9`, `--http09` | 使用 HTTP/0.9 风格请求 |
| `-1`, `--http10` | 使用 HTTP/1.0 |
| `-2`, `--http11` | 使用 HTTP/1.1，默认值；不支持 Keep-Alive |
| `--get` | 使用 GET 方法，默认值 |
| `--head` | 使用 HEAD 方法；HTTP/0.9 下会自动升级到 HTTP/1.0 |
| `--options` | 使用 OPTIONS 方法选项；HTTP/1.0 及以下会自动升级到 HTTP/1.1，实际请求方法存在已知问题 |
| `--trace` | 使用 TRACE 方法；HTTP/1.0 及以下会自动升级到 HTTP/1.1 |
| `-?`, `-h`, `--help` | 显示帮助信息 |
| `-V`, `--version` | 显示程序版本 |

注意：版本选项的短参数是大写 `-V`，不是小写 `-v`。

## URL 规则

直接请求目标服务器时：

- 仅支持 `http://`，不支持直接请求 `https://`。
- 主机名后必须包含路径，根路径也需要显式写成 `/`。
- 支持在主机名后指定端口。
- URL 最大长度为 1500 字节。
- 当前 Socket 实现仅支持 IPv4。

正确示例：

```text
http://example.com/
http://example.com/index.html
http://example.com:8080/api?id=1
```

以下地址会被判定为无效：

```text
example.com
https://example.com/
http://example.com
```

其中 `http://example.com` 缺少主机名后的 `/`，应改为 `http://example.com/`。

## 输出说明

测试开始前，程序会输出目标 URL、请求方法、HTTP 版本、客户端数和测试时长。测试结束后输出汇总结果：

```text
Speed=1234 pages/sec, 567890 bytes/sec.
Requests: 37000 ok, 10 http error, 20 failed.
```

以上数值仅为输出格式示例，各字段含义如下：

| 字段 | 含义 |
| --- | --- |
| `Speed pages/sec` | 平均每秒处理的请求数，计算时包含成功、HTTP 错误和失败请求 |
| `Speed bytes/sec` | 平均每秒从服务器接收的响应字节数 |
| `ok` | 成功请求数；普通模式下仅统计 HTTP 2xx 响应 |
| `http error` | HTTP 响应未通过 2xx 检查，或未读取到有效响应等异常数量 |
| `failed` | 连接、写入、读取或关闭 Socket 等操作失败的请求数 |

每个请求都会重新建立 TCP 连接。普通模式读取响应直到服务器关闭连接，HTTP/1.1 请求会携带 `Connection: close`。

## 退出状态码

| 状态码 | 含义 |
| --- | --- |
| `0` | 正常结束，或显示了帮助、版本信息 |
| `1` | 压测前的测试连接失败，目标服务器不可连接 |
| `2` | 命令行参数或 URL 无效 |
| `3` | 内部错误，例如内存分配或线程属性设置失败 |
| `4` | 设置系统资源限制失败 |

## 常见问题

### 提示 `ctags: command not found`

该提示不会影响程序编译。可以安装 ctags，也可以改用：

```bash
make superwebbench
```

### 提示 `Connect to server failed`

请检查：

- URL 是否符合上述格式。
- 目标服务是否已启动，端口是否正确。
- 本机能否解析目标主机名并建立 TCP 连接。
- 防火墙或网络策略是否阻止连接。

### 提示 `Set NOFILE/NPROC limit failed`

程序会根据客户端数尝试调整文件描述符和进程数限制。如果当前用户权限或系统硬限制不足，程序会退出。请降低 `--clients`，或根据操作系统要求调整 `ulimit -n`、`ulimit -u` 及相应系统限制。

## 工作原理

1. 解析命令行参数、目标主机、端口和请求路径。
2. 构造 HTTP 请求行及 `User-Agent`、`Host`、`Connection` 等请求头。
3. 根据并发客户端数检查并调整系统资源限制。
4. 建立一次测试连接，提前确认目标是否可访问。
5. 为每个客户端创建一个 POSIX 工作线程。
6. 工作线程循环执行“连接、发送请求、读取响应、关闭连接、更新计数”。
7. 主线程等待指定时长后汇总各线程计数并输出结果。

## 当前限制与已知问题

- 不支持直接请求 HTTPS，也不支持 IPv6。
- HTTP/1.1 不支持 Keep-Alive，每次请求都会重新建立连接。
- `--proxy` 参数解析后未正确设置后续连接使用的代理主机，代理模式目前不可用。
- `--options` 当前实际生成的请求方法为 `OPTION`，与标准 HTTP 方法 `OPTIONS` 不一致。
- 工作线程使用无限循环，没有显式停止标志和线程回收流程，测试结束后依赖进程退出终止线程。
- `make tar` 当前引用了仓库中不存在的 `debian/rules`，且复制清单遗漏 `socket.h`，不建议直接用于源码打包。

## 项目结构

```text
.
├── superwebbench.c    # 主程序、参数解析、HTTP 请求构造和压测逻辑
├── socket.c           # IPv4 地址解析与 TCP 连接
├── socket.h           # Socket 接口声明
├── Makefile           # 编译、安装、清理和打包规则
├── superwebbench.1    # man 手册
├── README             # 原始项目说明
├── README.md          # 中文使用说明
└── Changelog          # 历史变更记录
```

## 版权与许可证

- 原 Webbench 作者：Radim Kolar，1997–2004。
- SuperWebBench 修改作者：XuTongle，2012。
- `socket.c` 中的 UNIX Socket 代码取自 popclient 1.5，原始代码为 public domain，由 Virginia Tech Computing Center 创建。
- 主程序源码声明采用 **GNU General Public License version 2（GPL v2）**。

原始版权说明保留在源码、`README` 和 `superwebbench.1` 中。当前仓库未包含独立的 `LICENSE` 或 `COPYING` 许可证全文文件。
