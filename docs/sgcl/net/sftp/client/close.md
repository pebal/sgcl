[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::close

```cpp
expected<void, io::error> close() const noexcept;
```

Ends the session at once, and the SSH connection when [connect](connect.md) made it: the requests in progress
end with `io::errc::closed`, as does every call after it, of the client and of its files. A second close does
nothing.

## Parameters

None.

## Return value

Nothing.

## Complexity

Constant.

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
    fs.close();
    println("{} {}", fs.is_closed(), fs.stat("/").error().code() == io::errc::closed);
    srv.close();
}
```

Output:

```text
true true
```

## See also

- [is_closed](is_closed.md)
- [connect](connect.md)
- [sgcl::net::sftp::client](README.md)
