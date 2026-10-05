[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::close

```cpp
expected<void, io::error> close() const noexcept;
```

Ends the session at once (RFC 4254 §5.3, after the writes in progress): the reads and writes of its streams in
progress end, and every call after it is `io::errc::closed`. The program on the server's side is the server's to end:
OpenSSH's sends it SIGHUP.

## Parameters

None.

## Return value

Nothing.

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
    net::ssh::session s = c.open_session();
    s.exec("sleep");
    s.close();
    println("{}", s.input().write("x").error().code() == io::errc::closed);
    srv.close();
}
```

Output:

```text
true
```

## See also

- [wait](wait.md)
- [sgcl::net::ssh::session](README.md)
