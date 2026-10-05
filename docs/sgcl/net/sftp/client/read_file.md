[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::read_file, async_read_file

```cpp
expected<vector<byte>, io::error> read_file(const string& path) const;                                // (1)
async::task<expected<vector<byte>, io::error>> async_read_file(const string& path) const noexcept;    // (2)
```

The whole remote file at `path`, as [io::read_file](../../../io/read_file.md) reads a local one: opened, read
in requests of the server's largest read kept in flight together, closed.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file's path on the server |

## Return value

The file's bytes. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended.

## Complexity

Linear in the file's size; about one round trip per window of requests in flight.

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
    fs.write_file("/raw.bin", "\x01\x02\x03");
    vector<byte> data = fs.read_file("/raw.bin");
    println("{} {}", data.size(), int(data[2]));
    srv.close();
}
```

Output:

```text
3 3
```

## See also

- [read_text](read_text.md)
- [download](download.md)
- [write_file](write_file.md)
- [sgcl::net::sftp::client](README.md)
