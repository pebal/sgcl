[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& buffer) const;                                // (1)
async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept;    // (2)
```

Reads up to `buffer.size()` bytes at the position, which moves past them: one request, of at most the server's
largest read, so fewer bytes may come than the buffer holds; 0 at the end of the file.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

The algorithms of [mixin::reader](../../../io/mixin/reader/README.md) loop over it: `read_full` fills a buffer,
`read_all` reads to the end. A whole file goes faster by [client::read_file](../client/read_file.md) or
[client::download](../client/download.md), which keep many reads in flight.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

The number of bytes read, 0 at the end or for an empty buffer. Or the server's status as an [io::error](../../../io/error/README.md) (`is_not_found()`, `is_permission()`, `ENOTSUP`, `net::errc::sftp_failure` with the server's message), `io::errc::closed` for a file closed or a session ended.

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
    fs.write_file("/song.txt", "la la la");
    net::sftp::file f = fs.open("/song.txt");
    vector<byte> buf(5);
    size_t n = f.read(buf);
    println("{} {}", n, string(buf));
    println("{}", f.read_all_text().value());
    srv.close();
}
```

Output:

```text
5 la la
 la
```

## See also

- [read_at](read_at.md)
- [write](write.md)
- [mixin::reader](../../../io/mixin/reader/README.md)
- [sgcl::net::sftp::file](README.md)
