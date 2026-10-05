[sgcl](../../README.md) › [net](../README.md) › [sftp](README.md)

# sgcl::net::sftp::file_system_info

```cpp
#include "sgcl/net/sftp/types.h"   // or "sgcl/net/sftp.h"

namespace sgcl::net::sftp {
    struct file_system_info {
        uint64_t block_size = 0;
        uint64_t fragment_size = 0;
        uint64_t blocks = 0;
        uint64_t blocks_free = 0;
        uint64_t blocks_available = 0;
        uint64_t files = 0;
        uint64_t files_free = 0;
        uint64_t files_available = 0;
        uint64_t id = 0;
        uint64_t flags = 0;
        uint64_t max_name_length = 0;
    };
}
```

`net::sftp::file_system_info` is what the file system that holds a path on the server says of itself, the fields of
`statvfs(3)`, as [client::stat_fs](client/stat_fs.md) gives them (`statvfs@openssh.com`). The space is counted in
fragments: `blocks * fragment_size` bytes in all.

## Member objects

| Member | Description |
|---|---|
| `block_size` | the preferred size of a transfer |
| `fragment_size` | the unit `blocks` counts in |
| `blocks` | the size of the file system, in fragments |
| `blocks_free` | the fragments free |
| `blocks_available` | the fragments free to a user who is not root |
| `files` | the file nodes in all |
| `files_free` | the file nodes free |
| `files_available` | the file nodes free to a user who is not root |
| `id` | the file system's id |
| `flags` | 1: mounted read-only; 2: set-id bits ignored |
| `max_name_length` | the longest name of a file |

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
            (void)net::sftp::serve(s, "files");
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::sftp::client fs = net::sftp::client::connect(l.local_endpoint().to_string(), o);
    net::sftp::file_system_info info = fs.stat_fs("/");
    println("{} {}", info.max_name_length > 0, (info.flags & 1) == 0);
    srv.close();
}
```

Output:

```text
true true
```

## See also

- [client::stat_fs](client/stat_fs.md)
- [net::sftp](README.md)
