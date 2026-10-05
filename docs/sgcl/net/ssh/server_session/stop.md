[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::stop

```cpp
async::stop_token stop() const noexcept;
```

A stop token stopped when the client closes the session or the connection ends (or the server is closed): a handler that runs long, or waits on something else, watches it ([async::stop_token](../../../async/stop_token/README.md)).

## Parameters

None.

## Return value

The token.

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
using namespace std::chrono_literals;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        co_await s.stop().stopped();
        println("the client left");
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("wait");
    s.close();
    async::sleep(100ms).wait();
    srv.close();
}
```

Output:

```text
the client left
```

## See also

- [server::close](../server/close.md)
- [sgcl::net::ssh::server_session](README.md)
