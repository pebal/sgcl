[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const ssh::client& c);                                       // (1)
static expected<client, io::error> connect(const string& address,                                       // (2)
                                           const ssh::client::options& o = {});
static async::task<expected<client, io::error>> async_connect(ssh::client c) noexcept;                  // (3)
static async::task<expected<client, io::error>> async_connect(string address,                           // (4)
                                                              ssh::client::options o = {}) noexcept;
```

The SFTP session: a session opened on an SSH connection, its `sftp` subsystem started, the protocol's version 3
agreed (`INIT` and `VERSION`, at most 30 s), and the server's limits read when it offers `limits@openssh.com`.

- (1, 3) Over `c`, a connection the program made with [ssh::client::connect](../../ssh/client/connect.md); it stays
  the program's, and [close](close.md) closes the session only.
- (2, 4) The connection made here to `address` with the options `o`, as
  [ssh::client::connect](../../ssh/client/connect.md) makes it; [close](close.md) closes both.
- (1–2) On a thread, as [task::wait](../../../async/task/wait.md) is; a task awaits (3–4).

The server's extensions are kept: [rename](rename.md) replaces a target by `posix-rename@openssh.com`,
[hard_link](hard_link.md), [stat_fs](stat_fs.md) and [file::sync](../file/sync.md) need theirs, and
[has_extension](has_extension.md) says which there are.

## Parameters

| Parameter | Description |
|---|---|
| `c` | an SSH connection, authenticated |
| `address` | the server's `"host:port"` |
| `o` | how the SSH connection is made and authenticated ([ssh::client::options](../../ssh/client-options.md)) |

## Return value

The client. Or the [io::error](../../../io/error/README.md): the SSH connection's (2, 4); `net::errc::ssh_channel_refused` or `ssh_request_refused` for a session or a subsystem the server refused; `net::errc::sftp_protocol` for an answer that is not SFTP's, a version other than 3, or none within 30 s; the connection's error when it ended meanwhile.

## Complexity

The SSH connection (2, 4), then three round trips: the session's open, the subsystem's start, the version.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/sftp.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("files");  // the directory the server serves
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) {
        if (s.subsystem() == "sftp") {
            (void)net::sftp::serve(s, "files");
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::sftp::client fs = net::sftp::client::connect(l.local_endpoint().to_string(), o);
    println("{} {}", bool(fs), fs.has_extension("posix-rename@openssh.com"));
    srv.close();
}
```

Output:

```text
true true
```

## See also

- [ssh::client](../../ssh/client/README.md)
- [close](close.md)
- [sgcl::net::sftp::client](README.md)
