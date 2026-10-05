[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::input

```cpp
io::reader input() const;
```

What the client writes to the program, as an [io::reader](../../../io/reader/README.md): its bytes as they come, and its end (0) at the client's EOF or the session's close. The window it takes is given back as it is read.

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
        auto lines = io::buffered_reader(s.input());
        int n = 0;
        while (lines.read_line().value()) {
            ++n;
        }
        (void)s.output().write(to_string(n) + " lines");
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("wc -l");
    s.input().write("a\nb\nc\n");
    s.close_input();
    println("{}", s.output().read_all_text().value());
    srv.close();
}
```

Output:

```text
3 lines
```

## See also

- [output](output.md)
- [sgcl::net::ssh::server_session](README.md)
