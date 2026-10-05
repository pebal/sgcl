[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::run_result

```cpp
#include "sgcl/net/ssh/types.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    struct run_result {
        string out;
        string err;
        ssh::exit_status status;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::run_result` is what a command run in one line gave ([client::run](client/run.md)): its standard output and
standard error whole, and how it ended ([exit_status](exit_status.md)). A code other than 0 is a value here, not an
error: the program's failure is the command's answer, the error of `run` is the connection's.

## Member objects

| Member | Description |
|---|---|
| `out` | what the command wrote to its standard output |
| `err` | what it wrote to its standard error |
| `status` | how it ended: its code, or its signal |

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
    srv.handle([](net::ssh::server_session s) {
        (void)s.output().write("partial results\n");
        (void)s.error_output().write("disk full\n");
        (void)s.exit(1);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::run_result r = c.run("backup");
    print("{}{}", r.out, r.err);
    println("{}", r.status.code);
    srv.close();
}
```

Output:

```text
partial results
disk full
1
```

## See also

- [client::run](client/run.md), [exit_status](exit_status.md)
- [net::ssh](README.md)
