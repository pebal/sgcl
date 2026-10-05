[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::subsystem

```cpp
string subsystem() const noexcept;
```

The subsystem's name the client asked for (`sftp`); empty for a command or a shell.

## Parameters

None.

## Return value

The name, or an empty string.

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
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("[" + s.subsystem() + "]"); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.subsystem("sftp");
    println("{} {}", s.output().read_all_text().value(), c.run("ls")->out);
    srv.close();
}
```

Output:

```text
[sftp] []
```

## See also

- [kind](kind.md), [command](command.md)
- [sgcl::net::ssh::server_session](README.md)
