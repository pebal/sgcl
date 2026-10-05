[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::kind

```cpp
session_kind kind() const noexcept;
```

What the session runs: a command, the user's shell or a subsystem ([session_kind](../session_kind.md)), as the client started it.

## Parameters

None.

## Return value

The kind.

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
        const char* names[] = {"exec", "shell", "subsystem"};
        (void)s.output().write(names[int(s.kind())]);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session a = c.open_session();
    a.shell();
    net::ssh::session b = c.open_session();
    b.subsystem("sftp");
    println("{} {} {}", c.run("x")->out, a.output().read_all_text().value(), b.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
exec shell subsystem
```

## See also

- [session_kind](../session_kind.md)
- [sgcl::net::ssh::server_session](README.md)
