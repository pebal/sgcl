[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::write_file, async_write_file

```cpp
expected<void, io::error> write_file(const string& path, const slice<const byte>& data,               // (1)
                                     io::permissions p = io::permissions(0666)) const;
async::task<expected<void, io::error>> async_write_file(const string& path,                           // (2)
                                                        const slice<const byte>& data,
                                                        io::permissions p = io::permissions(0666))
    const noexcept;
```

The remote file at `path` made, or emptied, with `data`, as [io::write_file](../../../io/write_file.md) writes
a local one: opened, written in requests of the server's largest write kept in flight together, closed. A text
converts to `data`.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file's path on the server |
| `data` | its new content |
| `p` | the permissions of a file made |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended; what was written before an error stays.

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
    fs.write_file("/hello.txt", "Hello, SFTP");
    println("{}", fs.read_text("/hello.txt").value());
    srv.close();
}
```

Output:

```text
Hello, SFTP
```

## See also

- [read_file](read_file.md)
- [upload](upload.md)
- [create](create.md)
- [sgcl::net::sftp::client](README.md)
