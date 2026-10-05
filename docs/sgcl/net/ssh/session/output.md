[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::output

```cpp
io::reader output() const;
```

The program's standard output: an [io::reader](../../../io/reader/README.md) of what the program writes, as it comes,
and its end when the program's output ends (EOF) or the session closes. Over a terminal it holds the standard error
too. What comes waits in the session until it is read; the window it takes is given back as it is.

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
        (void)s.output().write("first line\nsecond line\n");
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("lines");
    auto lines = io::buffered_reader(s.output());
    while (auto line = lines.read_line().value()) {
        println("[{}]", *line);
    }
    srv.close();
}
```

Output:

```text
[first line]
[second line]
```

## See also

- [error_output](error_output.md)
- [input](input.md)
- [sgcl::net::ssh::session](README.md)
