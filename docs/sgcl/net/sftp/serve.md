[sgcl](../../README.md) › [net](../README.md) › [sftp](README.md)

# sgcl::net::sftp::serve, async_serve

```cpp
expected<void, io::error> serve(const ssh::server_session& s, const string& root,      // (1)
                                const server_options& o = {});
async::task<expected<void, io::error>> async_serve(ssh::server_session s,              // (2)
                                                   string root,
                                                   server_options o = {}) noexcept;
```

Serves the directory `root` over the session `s` as SFTP version 3, until the client ends it: the
directory is the client's `/`, a relative path is under `o.home`. Every path the client names is resolved within
`root`, element by element: a `..` stops at the root, an absolute path starts at it, a symlink is followed within it
(an absolute target from the root, at most 40 links), so that no request reaches a file outside, whatever the links
under the root say; a file is opened without following a last symlink the walk did not resolve. The requests are
served in the order they come, each a call of the system's (`open`, `pread`, `pwrite`, `rename` …) on a thread of
the blocking pool, with OpenSSH's extensions: `posix-rename@openssh.com`, `statvfs@openssh.com` and `fstatvfs`,
`fsync@openssh.com`, `hardlink@openssh.com`, `limits@openssh.com`, `lsetstat@openssh.com`, `expand-path@openssh.com`
and `home-directory`.

1. On the calling thread: a handler that returns `void` runs on the blocking pool and calls it.
2. In a task: a handler that returns `async::task<>` awaits it.

The handler decides who gets which directory, read-only or not, as it decides everything a session does: it calls
`serve` for a session whose [subsystem](../ssh/server_session/subsystem.md) is `sftp`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the session, its subsystem `sftp` |
| `root` | the directory served, which must be there |
| `o` | read-only, the limit of open handles, the home ([server_options](server_options.md)) |

## Return value

Nothing when the client ended the session. Or the [io::error](../../io/error/README.md): the system's for
a `root` that is not there or not a directory, `net::errc::sftp_protocol` for a client that broke the protocol (a
packet longer than 256 KB, a request before `INIT`), the session's error when the connection ended.

## Complexity

Linear in the requests and the bytes moved.

## Exceptions

- (1) `std::system_error` when a thread of the blocking pool cannot be started.
- (2) None.

## Notes

A file left open by a client is closed when the session ends. The ids of a listing's owner are
numbers: the long form of an entry (what `sftp`'s `ls -l` prints) names the user and group by their ids.

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
    fs.write_file("/../../escape.txt", "inside");
    println("{}", io::read_text("files/escape.txt").value());
    srv.close();
}
```

Output:

```text
inside
```

## See also

- [server_options](server_options.md)
- [ssh::server](../ssh/server/README.md)
- [client](client/README.md): the other side
