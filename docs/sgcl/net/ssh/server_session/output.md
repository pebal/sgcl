[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::output

```cpp
io::writer output() const;
```

The program's standard output, as an [io::writer](../../../io/writer/README.md): each write sent in packets within the client's window, waiting while it is shut. Its `close` sends the end of the output (EOF) before the handler returns; the handler's return sends it anyway.

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
    srv.handle([](net::ssh::server_session s) {
        io::writer out = s.output();
        (void)out.write("partial ");
        (void)out.write("output");
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("make test")->out);
    srv.close();
}
```

Output:

```text
partial output
```

## See also

- [error_output](error_output.md), [input](input.md)
- [sgcl::net::ssh::server_session](README.md)
