[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md)

# sgcl::net::webdav::client

```cpp
#include "sgcl/net/webdav/client.h"   // or "sgcl/net/webdav.h"

namespace sgcl::net::webdav {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::webdav::client` is a client of a WebDAV server's tree, the URL's path its root ("/" of its calls):
[list](list.md), [stat](stat.md), [read](read.md), [write](write.md), [mkdir](mkdir.md), [remove](remove.md),
[copy](copy.md), [move](move.md), [lock](lock.md) and [unlock](unlock.md), through an
[http::client](../../http/client/README.md).

## Rules

- A handle of one word: copies share the HTTP client and the lock token given by [if_token](if_token.md).
- A path names a resource under the root ("/docs/a.txt"); it is percent-encoded on the way.
- A failure is the errno a file system would give: `ENOENT` for a resource or a parent that is not there, `EEXIST`
  for one that is, `EACCES` for a refusal, `EBUSY` for a locked one; another status `net::errc::http_status`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | a client of a URL |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another client |
| [list, async_list](list.md) | the members of a collection |
| [stat, async_stat](stat.md) | a resource's properties |
| [read, async_read](read.md) | a file's bytes |
| [write, async_write](write.md) | a file made or replaced |
| [mkdir, async_mkdir](mkdir.md) | a collection made |
| [remove, async_remove](remove.md) | a file or a collection removed |
| [copy, async_copy](copy.md) | a copy |
| [move, async_move](move.md) | a move or a rename |
| [lock, async_lock](lock.md) | an exclusive write lock |
| [unlock, async_unlock](unlock.md) | a lock taken off |
| [if_token](if_token.md) | the lock token the changes present |
| [operator bool](operator_bool.md) | whether the handle holds a client |
| [operator==](operator_cmp.md) | whether two handles are the same client |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/webdav.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    // a directory served under /dav on the loopback
    io::mkdir_all("files/docs");
    io::write_file("files/hello.txt", "hello, world");
    net::webdav::server dav("files", {.prefix = "/dav"});
    net::http::server srv;
    srv.route("/dav/", [dav](net::http::request r, net::http::response_writer w) { return dav.async_serve(r, w); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::webdav::client c(string::concat("http://", l.local_endpoint().to_string(), "/dav/"));
    c.write("/docs/notes.txt", "remember the milk");
    auto members = c.list("/docs").value();
    for (auto& r : members) {
        println("{} {} bytes", r.path, r.size);
    }
    srv.close();
    serving.wait();
}
```

Output:

```text
/docs/notes.txt 17 bytes
```

## See also

- [server](../server/README.md)
- [resource](../resource.md)
