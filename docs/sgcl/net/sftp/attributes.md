[sgcl](../../README.md) › [net](../README.md) › [sftp](README.md)

# sgcl::net::sftp::attributes

```cpp
#include "sgcl/net/sftp/types.h"   // or "sgcl/net/sftp.h"

namespace sgcl::net::sftp {
    struct attributes {
        optional<uint64_t> size;
        optional<uint32_t> uid;
        optional<uint32_t> gid;
        optional<io::permissions> mode;
        optional<io::file_time> accessed;
        optional<io::file_time> modified;
    };
}
```

`net::sftp::attributes` is what a [client::set_stat](client/set_stat.md) or a [file::set_stat](file/set_stat.md)
changes: each field set is sent, the rest left as the file has it. Go's `sftp.Chtimes`, `Chown`, `Chmod` and
`Truncate` in one value.

## Rules

- SFTP version 3 sends the owner's two ids together and the two times together: one set alone is sent with the
  other as the file has it (the times read first).
- The times go in whole seconds.

## Member objects

| Member | Description |
|---|---|
| `size` | the new size: the file cut, or extended with zeros |
| `uid` | the new owner's user id |
| `gid` | the new owner's group id |
| `mode` | the new mode bits |
| `accessed` | the new time of the last access |
| `modified` | the new time of the last modification |

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
    fs.write_file("/log.txt", "0123456789");
    net::sftp::attributes a;
    a.size = 3;
    a.modified = io::file_time(std::chrono::seconds(1600000000));
    fs.set_stat("/log.txt", a);
    net::sftp::file_info info = fs.stat("/log.txt");
    auto since = std::chrono::duration_cast<std::chrono::seconds>(info.modified.time_since_epoch());
    println("{} {}", info.size, since.count());
    srv.close();
}
```

Output:

```text
3 1600000000
```

## See also

- [client::set_stat](client/set_stat.md), [file::set_stat](file/set_stat.md)
- [file_info](file_info/README.md): what the server says
- [net::sftp](README.md)
