[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::read_at, async_read_at

```cpp
expected<size_t, io::error> read_at(const slice<byte>& buffer, uint64_t offset) const;     // (1)
async::task<expected<size_t, io::error>> async_read_at(const slice<byte>& buffer,          // (2)
                                                       uint64_t offset) const noexcept;
```

Reads up to `buffer.size()` bytes at `offset`, the position untouched: one request, as [read](read.md) makes.
Several may run at once, from any number of tasks.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |
| `offset` | where in the file to read |

## Return value

The number of bytes read, 0 at or past the end. Or the server's status as an [io::error](../../../io/error/README.md) (`is_not_found()`, `is_permission()`, `ENOTSUP`, `net::errc::sftp_failure` with the server's message), `io::errc::closed` for a file closed or a session ended.

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
    fs.write_file("/alphabet", "abcdefghij");
    net::sftp::file f = fs.open("/alphabet");
    vector<byte> buf(3);
    f.read_at(buf, 7);
    println("{} {}", string(buf), *f.tell());
    srv.close();
}
```

Output:

```text
hij 0
```

## See also

- [write_at](write_at.md)
- [read](read.md)
- [sgcl::net::sftp::file](README.md)
