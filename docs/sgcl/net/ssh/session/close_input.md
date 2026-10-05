[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::close_input, async_close_input

```cpp
expected<void, io::error> close_input() const;                                // (1)
async::task<expected<void, io::error>> async_close_input() const noexcept;    // (2)
```

The end of the program's input (EOF, RFC 4254 §5.3), after what was written to [input](input.md): a program that
reads its input to its end ends then. Once: a second does nothing, and a write after it is `io::errc::closed`.

## Parameters

None.

## Return value

Nothing; or the connection's error.

## Complexity

Constant.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    srv.handle([](net::ssh::server_session s) { (void)s.output().write(to_string(s.input().read_all_text()->size())); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("wc -c");
    s.input().write("12345");
    s.close_input();
    println("{}", s.output().read_all_text().value());
    println("{}", s.input().write("late").error().code() == io::errc::closed);
    srv.close();
}
```

Output:

```text
5
true
```

## See also

- [input](input.md)
- [sgcl::net::ssh::session](README.md)
