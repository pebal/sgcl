[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md)

# sgcl::net::sftp::file_info

```cpp
#include "sgcl/net/sftp/types.h"   // or "sgcl/net/sftp.h"

namespace sgcl::net::sftp {
    struct file_info;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::sftp::file_info` is what an SFTP server says about a file (the protocol's attributes): the name, the size, the
type, the mode, the owner and the times, as [client::stat](../client/stat.md), [client::lstat](../client/lstat.md),
[client::read_dir](../client/read_dir.md) and [file::stat](../file/stat.md) give it. It is
[io::file_info](../../../io/file_info/README.md) with what SFTP adds: the owner's ids and the time of the last access.

## Rules

- A field the server did not send is zero; every server in use sends them all.
- The times come in whole seconds, as SFTP version 3 sends them.

## Member objects

| Member | Description |
|---|---|
| `string name` | the last element of the path, or a listing's entry; empty from [file::stat](../file/stat.md) |
| `uint64_t size` | the size in bytes; 0 by default |
| `io::file_type type` | the [io::file_type](../../../io/file_type.md), from the mode's type bits; `unknown` by default |
| `io::permissions mode` | the [io::permissions](../../../io/permissions.md), the set-id and sticky bits included; `none` by default |
| `uint32_t uid` | the owner's user id |
| `uint32_t gid` | the owner's group id |
| `io::file_time accessed` | the time of the last access |
| `io::file_time modified` | the time of the last modification |

## Member functions

| Function | Description |
|---|---|
| [is_regular](is_regular.md) | checks whether the file is a regular file |
| [is_directory](is_directory.md) | checks whether the file is a directory |
| [is_symlink](is_symlink.md) | checks whether the file is a symbolic link |

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
    fs.write_file("/photo.jpg", "not really a photo");
    fs.chmod("/photo.jpg", io::permissions(0640));
    net::sftp::file_info info = fs.stat("/photo.jpg");
    println("{}: {} bytes, mode {:o}", info.name, info.size, unsigned(info.mode));
    srv.close();
}
```

Output:

```text
photo.jpg: 18 bytes, mode 640
```

## See also

- [client::stat](../client/stat.md), [client::read_dir](../client/read_dir.md)
- [attributes](../attributes.md): what a set_stat changes
- [net::sftp](../README.md)
