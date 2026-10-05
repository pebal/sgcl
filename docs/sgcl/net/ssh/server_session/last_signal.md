[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::last_signal

```cpp
string last_signal() const noexcept;
```

The last signal the client sent to the program ([session::signal](../session/signal.md)), by its name without `SIG`; empty when none came. The handler decides what a signal does.

## Parameters

None.

## Return value

The signal's name, or an empty string.

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
        (void)s.input().read_all();  // until the client's EOF
        (void)s.output().write("signal: " + s.last_signal());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("work");
    s.signal("INT");
    s.close_input();
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
signal: INT
```

## See also

- [stop](stop.md)
- [sgcl::net::ssh::server_session](README.md)
