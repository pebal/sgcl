[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::symlink, async_symlink

```cpp
expected<void, io::error> symlink(const string& target, const string& link) const;          // (1)
async::task<expected<void, io::error>> async_symlink(const string& target,                  // (2)
                                                     const string& link) const noexcept;
```

Makes a symlink at `link` whose text is `target`, kept as it is: a relative target is resolved from the
link's directory when the link is followed. The arguments go in OpenSSH's order, which every server in use takes
(the draft's order is the other).

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `target` | the text of the link |
| `link` | the path of the link on the server |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended; a path that is there is `net::errc::sftp_failure`.

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
    fs.mkdir("/releases");
    fs.write_file("/releases/v2.txt", "version 2");
    fs.symlink("releases/v2.txt", "/current");
    println("{} -> {}", fs.read_link("/current").value(), fs.read_text("/current").value());
    srv.close();
}
```

Output:

```text
releases/v2.txt -> version 2
```

## See also

- [read_link](read_link.md)
- [lstat](lstat.md)
- [hard_link](hard_link.md)
- [sgcl::net::sftp::client](README.md)
