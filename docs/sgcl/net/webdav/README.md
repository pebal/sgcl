[sgcl](../../README.md) › [net](../README.md) › webdav

# sgcl::net::webdav

```cpp
#include "sgcl/net/webdav.h"   // namespace sgcl::net::webdav
```

WebDAV both ways (RFC 4918, classes 1 and 2): a [server](server/README.md) of a directory, an HTTP handler mounted on
a route of an [http::server](../http/server/README.md), and a [client](client/README.md) of a server's tree. What Go
has in `golang.org/x/net/webdav` outside its standard library; what Finder, Windows and `davfs2` mount.

## The rules

1. The server resolves every path within its root, as the [SFTP server](../sftp/README.md) does: a "..", in any
   spelling, and a symlink that leads out stay inside it; no request reaches a file outside.
2. Properties: the live ones of the tree (resourcetype, getcontentlength, getlastmodified, getetag, getcontenttype,
   displayname, creationdate, supportedlock, lockdiscovery); dead ones PROPPATCH sets are kept in memory, all of a
   request's or none. PROPFIND with Depth infinity is refused (403, `propfind-finite-depth`).
3. Locks: exclusive write locks in memory with their timeouts; a resource locked is changed only by a request whose
   If header names the token (423 otherwise).
4. The client's failures are the errno a file system would give: `ENOENT`, `EEXIST`, `EACCES`, `EBUSY` for a lock;
   another status is `net::errc::http_status`.
5. Every call of the client that waits has two forms, `list()` on a thread and `co_await async_list()` in a task.
6. [server](server/README.md) and [client](client/README.md) are handles of one word.

## Classes

| Class | Header | Description |
|---|---|---|
| [client](client/README.md) | `client.h` | a client of a server's tree: list, stat, read, write, mkdir, remove, copy, move, lock |
| [client_options](client_options.md) | `client.h` | the credentials |
| [resource](resource.md) | `types.h` | a resource as a PROPFIND tells of it |
| [server](server/README.md) | `server.h` | a WebDAV server of a directory, an HTTP handler |
| [server_options](server_options.md) | `server.h` | the prefix, read only, the body's limit, the authorization |

## See also

- [http](../http/README.md)
- [sftp](../sftp/README.md)
- [Benchmarks](../benchmarks.md)
- RFC 4918; `tests/net/webdav/` (curl, and a client of Python's)
