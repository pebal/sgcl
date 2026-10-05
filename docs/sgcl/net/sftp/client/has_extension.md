[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::has_extension

```cpp
bool has_extension(const string& name) const noexcept;
```

Whether the server offered the extension `name` in its `VERSION`: `posix-rename@openssh.com`,
`statvfs@openssh.com`, `fsync@openssh.com`, `hardlink@openssh.com`, `limits@openssh.com`, `lsetstat@openssh.com`,
`expand-path@openssh.com`, `home-directory` and the like.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the extension's name |

## Return value

`true` when the server offered it.

## Complexity

Linear in the extensions offered.

## Exceptions

None.

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
    println("{} {}", fs.has_extension("statvfs@openssh.com"), fs.has_extension("copy-data"));
    srv.close();
}
```

Output:

```text
true false
```

## See also

- [limits](limits.md)
- [rename](rename.md)
- [sgcl::net::sftp::client](README.md)
