[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::limits

```cpp
sftp::limits limits() const noexcept;
```

The server's limits as `limits@openssh.com` gave them when the session started: the largest packet, read and
write, and the handles a client may hold open; zeros from a server without the extension. The client keeps its
requests within them.

## Parameters

None.

## Return value

The [limits](../limits.md).

## Complexity

Constant.

## Exceptions

None.

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
    net::sftp::limits lim = fs.limits();
    println("{} {}", lim.max_read_length, lim.max_open_handles);
    srv.close();
}
```

Output:

```text
261120 256
```

## See also

- [has_extension](has_extension.md)
- [limits](../limits.md)
- [sgcl::net::sftp::client](README.md)
