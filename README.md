# SuperWebBench

[简体中文](README.md) | **English**

[![GitHub stars](https://img.shields.io/github/stars/xutongle/superwebbench?style=flat-square)](https://github.com/xutongle/superwebbench/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/xutongle/superwebbench?style=flat-square)](https://github.com/xutongle/superwebbench/network/members)
[![GitHub issues](https://img.shields.io/github/issues/xutongle/superwebbench?style=flat-square)](https://github.com/xutongle/superwebbench/issues)
[![GitHub license](https://img.shields.io/github/license/xutongle/superwebbench?style=flat-square)](LICENSE)

A command-line load testing tool refactored and extended from Webbench. Built with C11 and POSIX Threads, it simulates concurrent client requests, supports HTTP/HTTPS and IPv4/IPv6, and reports request success rates, HTTP errors, failed requests, QPS, and network throughput for web service performance testing and capacity planning.

Current version: **1.0.1**. See the [Changelog](Changelog) for detailed changes.

> Only run load tests against servers that you own or are explicitly authorized to test. High-concurrency requests can consume significant network, connection, and system resources, and may affect the target service and other workloads on the same machine.

## Features

- Simulates concurrent clients with POSIX threads, using a default thread stack size of 32 KiB.
- Stops worker threads gracefully when the test duration expires; the main thread reclaims them with `pthread_join` and fully aggregates all counters.
- Supports IPv4 and IPv6 targets, with thread-safe address resolution through `getaddrinfo`.
- Supports direct HTTPS testing by establishing TLS connections with OpenSSL, sending SNI, and validating the certificate chain and hostname or IP address.
- Provides GET, HEAD, OPTIONS, and TRACE request method options.
- Supports repeated `-H` / `--header` options for adding custom HTTP request headers.
- Supports HTTP/0.9, HTTP/1.0, and HTTP/1.1.
- Supports configurable concurrent client counts and test durations.
- Supports a `--force` mode that sends requests without waiting for responses.
- Provides proxy and proxy-cache bypass options.
- Checks HTTP response status in normal mode and counts only 2xx responses as successful.
- Reports successful requests, HTTP errors, failed requests, requests per second, and received bytes per second.
- Supports human-readable text output and script-friendly JSON output.

## Requirements

The project primarily targets Linux and other Unix-like systems and uses the following system interfaces and tools:

- A C compiler (`gcc` by default) and Make.
- POSIX Threads (pthread).
- POSIX/BSD sockets, `getaddrinfo` (IPv4/IPv6), and `getopt_long`.
- OpenSSL 1.1 or later (`libssl` and `libcrypto`, for HTTPS).
- Resource limit interfaces from `sys/resource.h`.
- C11 `<stdatomic.h>` for worker thread stop flags.

The source code has not been adapted for Windows. Compilation and operation have been verified on macOS (clang); on other Unix-like platforms, run `make` to verify compatibility.

The Makefile first uses `pkg-config` to obtain OpenSSL compiler and linker flags, falling back to `-lssl -lcrypto` if they are unavailable. If OpenSSL is installed in a non-standard location, specify `OPENSSL_CFLAGS` and `OPENSSL_LIBS` explicitly.

`ctags` is used only to generate the source index and is not required to compile or run the program. Before generating the index, the default `make` target checks whether `ctags` is available. If installed, it runs `ctags *.c`; otherwise, it prints a skip notice and continues without affecting compilation of the binary.

## Build and Installation

### Build

Run the following command in the project root directory:

```bash
make
```

The build produces the `superwebbench` executable.

To compile only the program without generating a ctags index:

```bash
make superwebbench
```

To clean build artifacts:

```bash
make clean
```

### Installation

Install under `/usr` by default:

```bash
sudo make install
```

You can also specify another installation prefix, for example:

```bash
sudo make install PREFIX=/usr/local
```

The installation includes:

- Executable: `$(PREFIX)/bin/superwebbench`
- Man page: `$(PREFIX)/share/man/man1/superwebbench.1`
- Project documentation: `$(PREFIX)/share/doc/superwebbench/`

After installation, view the command manual with:

```bash
man superwebbench
```

When building a package, use `DESTDIR` to install into a staging directory:

```bash
make install DESTDIR=/tmp/superwebbench-package PREFIX=/usr
```

### Source Packaging

The following command cleans build artifacts and then creates `superwebbench-1.0.1.tar.gz`:

```bash
make tar
```

### Automated Builds and Releases

GitHub Actions automatically builds on Ubuntu and macOS and runs smoke tests for the version and help commands. Pushing any tag creates a GitHub Release and uploads the source package and the `superwebbench-<tag>-linux-x86_64.tar.gz` binary package.

## Quick Start

View help and version information:

```bash
./superwebbench --help
./superwebbench --version
```

Test a target with the default configuration of 1 client running for 30 seconds:

```bash
./superwebbench http://127.0.0.1/
```

Run a 30-second test with 100 concurrent clients:

```bash
./superwebbench -c 100 -t 30 http://example.com/
```

Use the HEAD method and HTTP/1.0 with 50 clients for 10 seconds:

```bash
./superwebbench -c 50 -t 10 --head -1 http://example.com/
```

Test a non-default port:

```bash
./superwebbench -c 20 -t 10 http://127.0.0.1:8080/
```

Test an HTTPS address directly; port 443 is used when no port is specified:

```bash
./superwebbench -c 20 -t 10 https://example.com/
./superwebbench -c 20 -t 10 https://example.com:8443/
```

By default, HTTPS validates the CA certificate chain and the target hostname or IP address. When using a private CA, configure trusted certificates through the standard OpenSSL `SSL_CERT_FILE` or `SSL_CERT_DIR` environment variables.

Add custom request headers by repeating `-H` or `--header`:

```bash
./superwebbench \
  -H "Authorization: Bearer your-token" \
  -H "Content-Type: application/json" \
  -H "X-Request-Source: benchmark" \
  -c 20 -t 30 \
  https://example.com/api
```

Each option adds one header in `Name: Value` format. Custom headers are appended after the default headers in argument order, and fields with the same name are not deduplicated. Field names must follow the HTTP token rules, and values must not contain newlines or other control characters. HTTP/0.9 does not support request headers, so combining `--http09` with `-H` automatically upgrades the request to HTTP/1.0.

Send requests and close connections without reading responses:

```bash
./superwebbench -c 20 -t 10 --force http://127.0.0.1/
```

`--force` mode generates greater request pressure. Its success count indicates only that the request was sent and the connection was closed; it does not mean that the server returned an HTTP 2xx response.

Output results as JSON for parsing by scripts or CI:

```bash
./superwebbench -c 20 -t 10 --output json http://127.0.0.1:8080/
```

You can also save the results directly to a file:

```bash
./superwebbench -c 20 -t 10 -o json http://127.0.0.1:8080/ > result.json
```

## Command-Line Usage

```text
superwebbench [options] URL
```

| Option | Description |
| --- | --- |
| `-c`, `--clients <n>` | Number of concurrent clients; defaults to `1`. Non-numeric, negative, or zero values cause an error and exit with status 2 |
| `-t`, `--time <sec>` | Test duration in seconds; defaults to `30`. Non-numeric, negative, or zero values cause an error and exit with status 2 |
| `-f`, `--force` | Close the connection immediately after sending the request without waiting for a response |
| `-r`, `--reload` | Send a `Pragma: no-cache` request header when using a proxy; combining a proxy with HTTP/0.9 automatically upgrades the request to HTTP/1.0 |
| `-p`, `--proxy <server:port>` | Send requests through the specified proxy server `server:port` |
| `-o`, `--output <format>` | Result format: `text` (default) or `json`; other values cause an error and exit with status 2 |
| `-H`, `--header <name:value>` | Add a custom HTTP request header; may be repeated. Invalid formatting or control characters cause an exit with status 2 |
| `-9`, `--http09` | Use an HTTP/0.9-style request; automatically upgrades to HTTP/1.0 when custom request headers are specified |
| `-1`, `--http10` | Use HTTP/1.0 |
| `-2`, `--http11` | Use HTTP/1.1, the default; Keep-Alive is not supported |
| `--get` | Use the GET method, the default |
| `--head` | Use the HEAD method; automatically upgrades from HTTP/0.9 to HTTP/1.0 |
| `--options` | Use the OPTIONS method; automatically upgrades from HTTP/1.0 or earlier to HTTP/1.1 |
| `--trace` | Use the TRACE method; automatically upgrades from HTTP/1.0 or earlier to HTTP/1.1 |
| `-?`, `-h`, `--help` | Display help information |
| `-V`, `--version` | Display the program version |

Note: the short version option is uppercase `-V`, not lowercase `-v`.

## URL Rules

When requesting a target server directly:

- Both `http://` and `https://` are supported, with default ports 80 and 443 respectively.
- A path is required after the hostname; the root path must also be written explicitly as `/`.
- A port may be specified after the hostname and must be a number from 1 to 65535.
- IPv4 addresses, hostnames (which may resolve to AAAA records), and IPv6 literals in `[::1]` form are supported.
- The maximum URL length is 1500 bytes.

Valid examples:

```text
http://example.com/
https://example.com/
https://example.com:8443/api?id=1
http://example.com/index.html
http://example.com:8080/api?id=1
http://127.0.0.1:8080/
https://[::1]:8443/
```

The following addresses are considered invalid:

```text
example.com
ftp://example.com/
http://example.com
https://example.com
```

`http://example.com` and `https://example.com` are missing the `/` after the hostname; append `/` to each address.

## Output

Before the test starts, the program prints the target URL, request method, HTTP version, client count, and test duration. After the test finishes, it prints a summary:

```text
Speed=3008 requests/sec (total), 3008 pages/sec (success), 2972398 bytes/sec.
Requests: 6017 ok, 0 http error, 0 failed.
```

The values above are examples only. The fields have the following meanings:

| Field | Meaning |
| --- | --- |
| `requests/sec (total)` | Average total requests per second, including successful requests, HTTP errors, and failed requests; reflects overall throughput |
| `pages/sec (success)` | Average successful requests per second; counts only HTTP 2xx responses |
| `bytes/sec` | Average response bytes received from the server per second |
| `ok` | Number of successful requests; counts only HTTP 2xx responses in normal mode |
| `http error` | Number of HTTP responses that failed the 2xx check or other anomalies, such as failing to read a valid response |
| `failed` | Number of requests that failed during connection, write, read, socket close, or similar operations |

Each request establishes a new TCP connection, and each HTTPS request also performs a new TLS handshake. Normal mode reads the response until the server closes the connection. HTTP/1.1 requests include `Connection: close`.

### JSON Output

When `-o json` or `--output json` is used, standard output contains only one complete JSON object, with no copyright or progress text. Error messages are still written to standard error, and exit status rules remain unchanged.

```json
{"version":"1.0.1","url":"http://127.0.0.1:8080/","host":"127.0.0.1","port":8080,"method":"GET","http_version":"HTTP/1.1","clients":20,"duration_seconds":10,"force":false,"proxy":false,"reload":false,"total_requests":6017,"successful_requests":6017,"http_errors":0,"failed_requests":0,"bytes_received":29723980,"requests_per_second":601,"successful_requests_per_second":601,"bytes_per_second":2972398}
```

Field reference:

| JSON Field | Meaning |
| --- | --- |
| `total_requests` | Total requests, equal to the sum of successful requests, HTTP errors, and failed requests |
| `successful_requests` | Number of successful requests |
| `http_errors` | Number of HTTP error requests |
| `failed_requests` | Number of failed requests |
| `bytes_received` | Total response bytes received |
| `requests_per_second` | Average total requests per second |
| `successful_requests_per_second` | Average successful requests per second |
| `bytes_per_second` | Average received bytes per second |

The remaining fields record the load test configuration, including URL, host, port, method, HTTP version, client count, duration, and the force/proxy/reload switches. JSON strings escape quotation marks, backslashes, and control characters.

## Exit Status Codes

| Status | Meaning |
| --- | --- |
| `0` | Normal termination, or help/version information was displayed; unknown options or options missing required arguments also display help and return this status |
| `1` | The pre-test connection to the target failed, or the HTTPS TLS handshake or certificate validation failed |
| `2` | A command-line argument value or URL was invalid |
| `3` | An internal error occurred, such as a memory allocation, thread attribute setup, or TLS initialization failure |
| `4` | Failed to set system resource limits |

## Troubleshooting

### `ctags` Is Not Installed

The default `make` target prints `ctags not found, skipping tags generation` and automatically skips source index generation without affecting compilation. Install ctags if you need the index, or compile only the program:

```bash
make superwebbench
```

### `Connect to server failed` Is Reported

Check the following:

- Whether the URL follows the format described above.
- Whether the target service is running and the port is correct.
- Whether the local machine can resolve the target hostname and establish a TCP connection.
- Whether the HTTPS certificate was issued by a trusted CA and its hostname or IP address matches the URL.
- Whether a firewall or network policy is blocking the connection.

### `Set NOFILE/NPROC limit failed` Is Reported

The program attempts to adjust file descriptor and process count limits based on the number of clients. If the current user's permissions or the system's hard limits are insufficient, the program exits. Reduce `--clients`, or adjust `ulimit -n`, `ulimit -u`, and the corresponding system limits as required by your operating system.

## How It Works

1. Parses command-line arguments, the target host, port, and request path.
2. Builds the HTTP request line and default headers such as `User-Agent`, `Host`, and `Connection`, then appends custom request headers.
3. Checks and adjusts system resource limits based on the concurrent client count.
4. Establishes one test connection to confirm that the target is reachable; HTTPS also completes a TLS handshake and certificate validation.
5. Creates one POSIX worker thread per client.
6. Each worker repeatedly connects, sends a request, reads the response, closes the connection, and updates counters. HTTPS connections establish a TLS session before sending the request.
7. When the specified duration expires, the main thread sets the stop flag, waits for each worker to finish its current request and exit (`pthread_join`), then aggregates the counters and prints the results. If the target server does not respond for a long time, shutdown waits for in-progress requests to complete.

## Current Limitations and Known Issues

- `--proxy` sends HTTP proxy requests in the original style and does not support HTTPS tunneling through `CONNECT`; do not specify this option when testing HTTPS directly.
- HTTP/1.1 does not support Keep-Alive; each request establishes a new connection.
- The request line and all request headers share a 2048-byte internal buffer. An oversized request causes an error and exits with status 2.
- HTTP/0.9 responses have no status line, so they cannot pass the 2xx status check in normal mode and are usually counted as `http error`; `--force` mode does not read or validate responses.

## Project Structure

```text
.
├── .github/workflows/
│   ├── ci.yml         # Ubuntu/macOS builds and smoke tests
│   └── release.yml    # Tag-triggered GitHub Release
├── superwebbench.c    # Main program, argument parsing, HTTP request construction, and load testing logic
├── socket.c           # IPv4/IPv6 TCP connections and OpenSSL TLS transport
├── socket.h           # Socket and TLS connection interface declarations
├── Makefile           # Build, installation, cleanup, and packaging rules
├── superwebbench.1    # Man page
├── README             # Original project description
├── README.md          # Chinese documentation
├── README_EN.md       # English documentation
├── Changelog          # Historical changes
├── LICENSE            # Full GNU GPL v2 license text
├── .gitattributes     # Git export attributes
└── .gitignore         # Git ignore rules
```

## Copyright and License

- Original Webbench author: Radim Kolar, 1997–2004.
- SuperWebBench modifications: XuTongle, 2012.
- The UNIX socket code in `socket.c` was taken from popclient 1.5. The original code is public domain and was created by the Virginia Tech Computing Center.
- The main program source code is licensed under the **GNU General Public License version 2 (GPL v2)**.

The original copyright notices are preserved in the source code, `README`, and `superwebbench.1`. See [LICENSE](LICENSE) for the full license text.
