[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::env

```cpp
vector<pair<string, string>> env() const noexcept;
```

The environment variables the client set before the start, as names and values in their order (at most 256 are kept). The handler decides what they mean.

## Parameters

None.

## Return value

The variables.

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
#include <string>

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) {
        for (auto& [name, value] : s.env()) {
            (void)s.output().write(name + "=" + value + ";");
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.set_env("LANG", "C");
    s.set_env("EDITOR", "vi");
    s.exec("env");
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
LANG=C;EDITOR=vi;
```

## See also

- [session::set_env](../session/set_env.md)
- [sgcl::net::ssh::server_session](README.md)
