[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::error_output

```cpp
io::reader error_output() const;
```

The program's standard error: an [io::reader](../../../io/reader/README.md) of what the program writes to it (the
channel's extended data of type 1, RFC 4254 §5.2), apart from its output. A program run in a terminal writes it into
the terminal, to [output](output.md).

## Parameters

None.

## Return value

The reader.

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
        (void)s.output().write("result");
        (void)s.error_output().write("warning");
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("job");
    println("{} / {}", s.output().read_all_text().value(), s.error_output().read_all_text().value());
    srv.close();
}
```

Output:

```text
result / warning
```

## See also

- [output](output.md)
- [sgcl::net::ssh::session](README.md)
