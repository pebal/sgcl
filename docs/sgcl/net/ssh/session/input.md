[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::input

```cpp
io::writer input() const;
```

The program's standard input: an [io::writer](../../../io/writer/README.md) whose writes go to the session's channel
within the server's window (each waiting while the window is shut), and whose `close` sends the end (EOF), as
[close_input](close_input.md) does. Every call gives a writer of the same channel; writes from two of them are taken one
after the other, whole.

## Parameters

None.

## Return value

The writer.

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
    srv.handle([](net::ssh::server_session s) { (void)s.output().write(s.input().read_all_text().value()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("cat");
    io::writer in = s.input();
    in.write("one, ");
    in.write("two");
    in.close();
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
one, two
```

## See also

- [close_input](close_input.md)
- [output](output.md)
- [sgcl::net::ssh::session](README.md)
