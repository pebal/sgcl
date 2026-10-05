[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::error_output

```cpp
io::writer error_output() const;
```

The program's standard error, as an [io::writer](../../../io/writer/README.md): the channel's extended data (RFC 4254 §5.2), which the client reads apart from the output.

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
        (void)s.output().write("data");
        (void)s.error_output().write("diagnostics");
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::run_result r = c.run("job");
    println("{} | {}", r.out, r.err);
    srv.close();
}
```

Output:

```text
data | diagnostics
```

## See also

- [output](output.md)
- [sgcl::net::ssh::server_session](README.md)
