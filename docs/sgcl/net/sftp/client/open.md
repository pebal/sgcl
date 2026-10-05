[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::open, async_open

```cpp
expected<file, io::error> open(const string& path,                                                // (1)
                               io::open_flags flags = io::open_flags::read,
                               io::permissions p = io::permissions(0666)) const;
async::task<expected<file, io::error>> async_open(const string& path,                             // (2)
                                                  io::open_flags flags = io::open_flags::read,
                                                  io::permissions p = io::permissions(0666))
    const noexcept;
```

Opens the file at `path` on the server, as [io::open](../../../io/open.md) opens a local one: `flags` are io's
(`read`, `write`, `append`, `create`, `truncate`, `exclusive`; `sync` is not sent), `p` the permissions of a file
made, which the server's umask narrows. The [file](../file/README.md) starts at position 0; with `append`, every
write goes to the end.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file's path on the server; a relative one is under the session's directory |
| `flags` | how it is opened ([io::open_flags](../../../io/open_flags.md)) |
| `p` | the permissions of a file made |

## Return value

The open file. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended.

## Complexity

One round trip.

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
    fs.write_file("/notes.txt", "one\n");
    net::sftp::file f = fs.open("/notes.txt", io::open_flags::write | io::open_flags::append);
    f.write("two\n");
    f.close();
    print("{}", fs.read_text("/notes.txt").value());
    srv.close();
}
```

Output:

```text
one
two
```

## See also

- [create](create.md)
- [file](../file/README.md)
- [io::open_flags](../../../io/open_flags.md)
- [sgcl::net::sftp::client](README.md)
