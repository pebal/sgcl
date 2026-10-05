[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::stat, async_stat

```cpp
expected<file_info, io::error> stat() const;                                // (1)
async::task<expected<file_info, io::error>> async_stat() const noexcept;    // (2)
```

What the server says about the open file (`FSTAT`), as a [file_info](../file_info/README.md); its name is
empty, since a handle has none.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

None.

## Return value

The attributes. Or the server's status as an [io::error](../../../io/error/README.md) (`is_not_found()`, `is_permission()`, `ENOTSUP`, `net::errc::sftp_failure` with the server's message), `io::errc::closed` for a file closed or a session ended.

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
    net::sftp::file f = fs.create("/growing.log");
    f.write("12345678");
    println("{}", f.stat()->size);
    srv.close();
}
```

Output:

```text
8
```

## See also

- [client::stat](../client/stat.md)
- [set_stat](set_stat.md)
- [sgcl::net::sftp::file](README.md)
