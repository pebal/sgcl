[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::close

```cpp
expected<void, io::error> close() const noexcept;
```

Ends the connection at once: the transport closed, every session, forwarded connection and listener over it ended,
the calls in progress ended with `io::errc::closed`, and every call after it too. A second close does nothing.

## Parameters

None.

## Return value

Nothing.

## Complexity

Linear in the channels open.

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
    net::ssh::session s = c.open_session();
    c.close();
    println("{} {}", c.is_closed(), s.exec("late").error().code() == io::errc::closed);
    srv.close();
}
```

Output:

```text
true true
```

## See also

- [wait](wait.md), [is_closed](is_closed.md)
- [sgcl::net::ssh::client](README.md)
