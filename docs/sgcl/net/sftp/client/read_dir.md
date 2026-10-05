[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::read_dir, async_read_dir

```cpp
expected<vector<file_info>, io::error> read_dir(const string& path) const;                // (1)
async::task<expected<vector<file_info>, io::error>> async_read_dir(const string& path)    // (2)
    const noexcept;
```

The entries of the directory at `path`, each with its attributes, without `.` and `..`, in the server's order
(`OPENDIR`, `READDIR` until its end, `CLOSE`).

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the directory's path on the server |

## Return value

The entries. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended; a path that is not a directory is not found, as OpenSSH answers.

## Complexity

Linear in the entries: a round trip per batch the server sends, and two more.

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

#include <algorithm>

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
    fs.mkdir("/photos");
    fs.write_file("/photos/a.jpg", "aaa");
    fs.write_file("/photos/b.jpg", "bb");
    vector<net::sftp::file_info> entries = fs.read_dir("/photos");
    std::sort(entries.begin(), entries.end(), [](auto& x, auto& y) { return x.name < y.name; });
    for (const net::sftp::file_info& e : entries) {
        println("{} {}", e.name, e.size);
    }
    srv.close();
}
```

Output:

```text
a.jpg 3
b.jpg 2
```

## See also

- [stat](stat.md)
- [mkdir](mkdir.md)
- [sgcl::net::sftp::client](README.md)
