[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::rename, async_rename

```cpp
expected<void, io::error> rename(const string& from, const string& to) const;            // (1)
async::task<expected<void, io::error>> async_rename(const string& from,                  // (2)
                                                    const string& to) const noexcept;
```

Renames the file or directory `from` to `to`, as [io::rename](../../../io/rename.md) does locally: a file at
`to` is replaced. The server is sent `posix-rename@openssh.com` when it offers it; one without it is sent SFTP's
`RENAME`, which refuses a `to` that is there.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `from` | the present path on the server |
| `to` | the new path |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended.

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
    fs.write_file("/draft.txt", "new");
    fs.write_file("/final.txt", "old");
    fs.rename("/draft.txt", "/final.txt");
    println("{} {}", fs.read_text("/final.txt").value(), *fs.exists("/draft.txt"));
    srv.close();
}
```

Output:

```text
new false
```

## See also

- [has_extension](has_extension.md)
- [sgcl::net::sftp::client](README.md)
