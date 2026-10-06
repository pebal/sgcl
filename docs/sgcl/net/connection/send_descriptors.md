[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::send_descriptors, async_send_descriptors

```cpp
expected<size_t, io::error> send_descriptors(const slice<const byte>& data,                                     // (1)
                                             const slice<const int>& fds) const;
async::task<expected<size_t, io::error>> async_send_descriptors(const slice<const byte>& data,                  // (2)
                                                                const slice<const int>& fds) const noexcept;
```

Passes descriptors to the process at the other end of a unix-domain connection (`SCM_RIGHTS`): the bytes of `data`
with the descriptors `fds` — a file's ([io::file::fd](../../io/file/fd.md)), a socket's ([fd](fd.md)), a pipe's end —
which the receiver gets as descriptors of its own, the same open files, their positions shared; the sender's stay its
own. Go's `UnixConn.WriteMsgUnix` with `syscall.UnixRights`. A stream carries descriptors with bytes only, so `data`
is not empty; the descriptors go in the first message, and the rest of the bytes, if it took part of them, as a write
sends them. The connection's write is taken as a [write](write.md) takes it, its write deadline holds.

1. On the calling thread, which waits while the socket is full.
2. The same for a task, waiting on the [reactor](../../async/readable.md). The task copies `fds` before it starts:
   the array they came from may go before the task runs.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes, not empty |
| `fds` | the descriptors, 1 to 253: an array, a `vector<int>`, a `std::array` |

## Return value

The bytes written, the size of `data`. Or the [error](../../io/error/README.md), its operation `send_descriptors`:
`EINVAL` for empty bytes, no descriptor or more than 253, `EBADF` for a descriptor that is not open,
`EOPNOTSUPP` for a TCP or TLS connection or one in memory, `io::errc::closed` for a closed one.

## Complexity

One `sendmsg`, linear in the bytes.

## Exceptions

- (1) None but what a blocking wait of the reactor throws (`std::system_error` when its thread cannot be made).
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::listener l = net::unix_domain::listen("pass.sock");
    net::connection a = net::unix_domain::connect("pass.sock");
    net::connection b = l.accept();
    io::write_file("shared.txt", "one open file, two processes");
    io::file f = io::open("shared.txt").value();
    int fds[] = {f.fd()};
    a.send_descriptors("here", fds).value();
    vector<byte> buffer(16);
    net::connection::received got = b.receive_descriptors(buffer).value();
    println("{} bytes, {} file", got.size, got.files.size());
    println("{}", got.files[0].read_all_text().value());
    l.close();
}
```

Output:

```text
4 bytes, 1 file
one open file, two processes
```

## See also

- [receive_descriptors](receive_descriptors.md): the other side
- [fd](fd.md): a socket's descriptor
- [unix_domain](../unix_domain/README.md): the connections that carry descriptors
- [sgcl::net::connection](README.md)
