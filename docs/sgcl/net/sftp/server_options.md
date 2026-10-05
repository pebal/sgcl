[sgcl](../../README.md) › [net](../README.md) › [sftp](README.md)

# sgcl::net::sftp::server_options

```cpp
#include "sgcl/net/sftp/types.h"   // or "sgcl/net/sftp.h"

namespace sgcl::net::sftp {
    struct server_options {
        bool read_only = false;
        uint32_t max_handles = 256;
        string home = "/";
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::sftp::server_options` is how [serve](serve.md) serves a directory: read-only or not, how many files a client may
hold open, and where its home is. The handler gives each user the options it decides on, as it gives the directory.

## Member objects

| Member | Description |
|---|---|
| `read_only` | every request that would change a file refused with "permission denied"; `false` by default |
| `max_handles` | the files and directories a client may hold open at once, refused past it; 256 by default |
| `home` | the user's home, a path under the served directory: what `home-directory` gives and `~` stands for in `expand-path@openssh.com`; `/` by default |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/sftp.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("files");  // the directory the server serves
    (void)io::write_file("files/notice.txt", "read me");
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) {
        if (s.subsystem() == "sftp") {
            net::sftp::server_options so;
            so.read_only = true;
            (void)net::sftp::serve(s, "files", so);
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::sftp::client fs = net::sftp::client::connect(l.local_endpoint().to_string(), o);
    println("{}", fs.write_file("/new.txt", "x").error().is_permission());
    println("{}", fs.read_text("/notice.txt").value());
    srv.close();
}
```

Output:

```text
true
read me
```

## See also

- [serve](serve.md)
- [net::sftp](README.md)
