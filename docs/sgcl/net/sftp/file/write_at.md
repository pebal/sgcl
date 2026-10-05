[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::write_at, async_write_at

```cpp
expected<size_t, io::error> write_at(const slice<const byte>& data, uint64_t offset) const;    // (1)
async::task<expected<size_t, io::error>> async_write_at(const slice<const byte>& data,         // (2)
                                                        uint64_t offset) const noexcept;
```

Writes the whole of `data` at `offset`, the position untouched, in requests in flight together as
[write](write.md) sends them. A server writes a file opened with `append` at its end whatever the offset.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |
| `offset` | where in the file to write |

## Return value

`data.size()`. Or the server's status as an [io::error](../../../io/error/README.md) (`is_not_found()`, `is_permission()`, `ENOTSUP`, `net::errc::sftp_failure` with the server's message), `io::errc::closed` for a file closed or a session ended.

## Complexity

Linear in the size of `data`.

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
    fs.write_file("/greeting", "hello world");
    net::sftp::file f = fs.open("/greeting", io::open_flags::write);
    f.write_at("WORLD", 6);
    f.close();
    println("{}", fs.read_text("/greeting").value());
    srv.close();
}
```

Output:

```text
hello WORLD
```

## See also

- [read_at](read_at.md)
- [write](write.md)
- [sgcl::net::sftp::file](README.md)
