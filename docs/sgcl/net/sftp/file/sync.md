[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::sync, async_sync

```cpp
expected<void, io::error> sync() const;                                // (1)
async::task<expected<void, io::error>> async_sync() const noexcept;    // (2)
```

Waits until what was written to the file reaches the server's disk (`fsync@openssh.com`), as
[io::file::sync](../../../io/file/sync.md) does locally.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

None.

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md) (`is_not_found()`, `is_permission()`, `ENOTSUP`, `net::errc::sftp_failure` with the server's message), `io::errc::closed` for a file closed or a session ended; `ENOTSUP` from a server without the extension.

## Complexity

One round trip and the server's `fsync`.

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
    net::sftp::file f = fs.create("/ledger");
    f.write("entry 1\n");
    println("{}", f.sync().has_value());
    srv.close();
}
```

Output:

```text
true
```

## See also

- [close](close.md)
- [client::has_extension](../client/has_extension.md)
- [sgcl::net::sftp::file](README.md)
