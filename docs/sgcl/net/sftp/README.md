[sgcl](../../README.md) › [net](../README.md) › sftp

# sgcl::net::sftp

```cpp
#include "sgcl/net/sftp.h"   // namespace sgcl::net::sftp
```

SFTP, the file transfer protocol of SSH, version 3 as OpenSSH speaks it (draft-ietf-secsh-filexfer-02 with OpenSSH's
extensions), both sides, over the module's [ssh](../ssh/README.md). A [client](client/README.md) opens the `sftp`
subsystem on an SSH connection and works on the server's files as [io](../../io/README.md) works on local ones: files
opened with io's flags and read, written and seeked as io's streams ([file](file/README.md)), the attributes read and
set, directories listed, made and removed, renames, links, and whole files in one line —
[read_file](client/read_file.md), [write_file](client/write_file.md), [upload](client/upload.md),
[download](client/download.md) — with many requests in flight at once, so that a transfer is as fast as the
connection.

The server's side is one function: [serve](serve.md) serves a directory over a session of
[ssh::server](../ssh/server/README.md), the directory as the client's `/`. Every path a client names is resolved
within it, element by element, as a chroot would: a `..` stops at the root, an absolute path starts at it, a symlink
is followed within it, its absolute target taken from the root, so that no request reaches a file outside, whatever
the links inside say. Who may see which directory, read-only or not, is the handler's to decide per user, as it
decides everything a session does. Interoperability was tested against OpenSSH 10.2: `sftp` against the server,
the client against `sftp-server` behind `sshd`.

## The rules

1. A client is a session of its own over an SSH connection: one made by the program
   ([connect](client/connect.md) with an [ssh::client](../ssh/client/README.md)) stays the program's, one made by
   `connect` with an address is closed with the client. The connection may carry other sessions at once.
2. The requests of many tasks and threads go out together over one session, each answered by its id: a client is
   shared as freely as an [ssh::client](../ssh/client/README.md).
3. A [file](file/README.md) reads and writes at offsets: its position is the client's, kept in the handle, and a
   [seek](file/seek.md) sends nothing. [read](file/read.md) is one request; [write](file/write.md) and the whole-file
   calls keep many in flight, within the server's limits (`limits@openssh.com`).
4. Every call that waits has two forms, as every call of the module: `fs.stat(...)` on a thread, `co_await
   fs.async_stat(...)` in a task.
5. Errors are values, `expected<T, io::error>`, with io's predicates: a server's "no such file" is `ENOENT`
   (`is_not_found()`), "permission denied" `EACCES` (`is_permission()`), "operation unsupported" `ENOTSUP`; SFTP's
   other failures are [errc](../errc.md)'s `sftp_failure`, the server's message after the path, and an answer that
   breaks the protocol `sftp_protocol`.
6. The server answers as OpenSSH's `sftp-server` does, in the order the requests come: a path that names no file, a
   symlink loop or a file where a directory should be is "no such file"; [server_options](server_options.md)
   `read_only` refuses every change, `max_handles` bounds the files and directories a client holds open.
7. Not here: the versions after 3 (4 to 6, which no server in use speaks), `copy-data`, `users-groups-by-id@openssh.com`
   and the other OpenSSH extensions of a later date, and a hook per request on the server's side (the handler gives
   each user a root and a mode, which is what a hook was for).

## Functions

| Function | Header | Description |
|---|---|---|
| [serve, async_serve](serve.md) | `server.h` | a directory served over an SSH session, every path kept within it |

## Classes

| Class | Header | Description |
|---|---|---|
| [attributes](attributes.md) | `types.h` | the attributes a set_stat changes |
| [client](client/README.md) | `client.h` | an SFTP session: files, attributes, directories, names, whole files |
| [file](file/README.md) | `client.h` | a remote file open, read and written as io's streams |
| [file_info](file_info/README.md) | `types.h` | what the server says about a file |
| [file_system_info](file_system_info.md) | `types.h` | what the server's file system says of itself |
| [limits](limits.md) | `types.h` | the server's limits of a packet, a read, a write, the handles |
| [server_options](server_options.md) | `types.h` | how a directory is served |

## See also

- [ssh](../ssh/README.md): the connections, the sessions and the server under it
- [io](../../io/README.md): the local files and streams the client mirrors
- draft-ietf-secsh-filexfer-02; OpenSSH's PROTOCOL (its SFTP extensions); `tests/net/sftp/`, against OpenSSH
