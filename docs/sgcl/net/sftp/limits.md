[sgcl](../../README.md) › [net](../README.md) › [sftp](README.md)

# sgcl::net::sftp::limits

```cpp
#include "sgcl/net/sftp/types.h"   // or "sgcl/net/sftp.h"

namespace sgcl::net::sftp {
    struct limits {
        uint64_t max_packet_length = 0;
        uint64_t max_read_length = 0;
        uint64_t max_write_length = 0;
        uint64_t max_open_handles = 0;
    };
}
```

`net::sftp::limits` is what an SFTP server says it takes (`limits@openssh.com`), as [client::limits](client/limits.md)
gives it: the client keeps its requests within them. A zero is a limit the server did not give.

## Member objects

| Member | Description |
|---|---|
| `max_packet_length` | the largest packet the server reads, its length field included |
| `max_read_length` | the most bytes a read gives |
| `max_write_length` | the most bytes a write takes |
| `max_open_handles` | the files and directories a client may hold open; 0 for no limit |

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
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) {
        if (s.subsystem() == "sftp") {
            net::sftp::server_options so;
            so.max_handles = 16;
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
    net::sftp::limits lim = fs.limits();
    println("{} {}", lim.max_write_length, lim.max_open_handles);
    srv.close();
}
```

Output:

```text
261120 16
```

## See also

- [client::limits](client/limits.md)
- [server_options](server_options.md): the server's side
- [net::sftp](README.md)
