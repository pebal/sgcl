[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::upload, async_upload

```cpp
expected<uint64_t, io::error> upload(const string& local, const string& remote) const;           // (1)
async::task<expected<uint64_t, io::error>> async_upload(const string& local,                     // (2)
                                                        const string& remote) const noexcept;
```

Sends the local file `local` to the path `remote` on the server, made or emptied, what `sftp`'s `put` does:
read from the disk in blocks while the server's writes are in flight together, so the transfer is as fast as the
connection and the two disks let it be.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `local` | the local file's path |
| `remote` | the path on the server |

## Return value

The bytes sent. Or the [io::error](../../../io/error/README.md) of the local file, or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended.

## Complexity

Linear in the file's size.

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
    (void)io::write_file("backup.tar", string(100000, 'x'));
    println("{}", fs.upload("backup.tar", "/backup.tar").value());
    println("{}", fs.stat("/backup.tar")->size);
    srv.close();
}
```

Output:

```text
100000
100000
```

## See also

- [download](download.md)
- [write_file](write_file.md)
- [sgcl::net::sftp::client](README.md)
