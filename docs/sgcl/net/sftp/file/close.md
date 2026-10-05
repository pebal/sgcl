[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::close, async_close

```cpp
expected<void, io::error> close() const;                                // (1)
async::task<expected<void, io::error>> async_close() const noexcept;    // (2)
```

Lets go of the server's handle (`CLOSE`): no operation starts after it, and they end with
`io::errc::closed`. A second close does nothing. A file not closed holds its handle until the session ends; the
server's limit of open handles counts it.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

None.

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md), or the session's error.

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
    net::sftp::file f = fs.create("/once.txt");
    f.close();
    println("{} {}", f.is_closed(), f.write("late").error().code() == io::errc::closed);
    srv.close();
}
```

Output:

```text
true true
```

## See also

- [is_closed](is_closed.md)
- [client::close](../client/close.md)
- [sgcl::net::sftp::file](README.md)
