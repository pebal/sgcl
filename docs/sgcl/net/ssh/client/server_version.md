[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::server_version

```cpp
string server_version() const noexcept;
```

The server's version line (RFC 4253 §4.2), without its CR LF: `SSH-2.0-OpenSSH_10.2` for OpenSSH, `SSH-2.0-SGCL_1.0` for the module's server.

## Parameters

None.

## Return value

The line.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.server_version());
    srv.close();
}
```

Output:

```text
SSH-2.0-SGCL_1.0
```

## See also

- [sgcl::net::ssh::client](README.md)
