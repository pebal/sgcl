[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::receive_descriptors, async_receive_descriptors

```cpp
expected<received, io::error> receive_descriptors(const slice<byte>& buffer, size_t max = 16) const;     // (1)
async::task<expected<received, io::error>> async_receive_descriptors(const slice<byte>& buffer,          // (2)
                                                                     size_t max = 16) const noexcept;
```

Reads the bytes that came on a unix-domain connection, at most the size of `buffer`, and the descriptors that came
with them ([send_descriptors](send_descriptors.md)), at most `max`: each an [io::file](../../io/file/README.md) the
program owns from now on, close-on-exec, which closes it ([close](../../io/file/close.md)) when it is done. Go's
`UnixConn.ReadMsgUnix` with `syscall.ParseUnixRights`. Bytes that came without descriptors are read with none. The
connection's read is taken as a [read](read.md) takes it, its read deadline holds; the read is beside
[read_line](read_line.md)'s buffer, so a buffer holding bytes refuses it rather than skip them.

1. On the calling thread, which waits until something comes.
2. The same for a task, waiting on the [reactor](../../async/readable.md).

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go; its size is the most read |
| `max` | the most descriptors taken (at most 253) |

## Return value

What was read ([received](../connection-received.md)): the bytes, 0 at the end of the stream, and the files. Or the
[error](../../io/error/README.md), its operation `receive_descriptors`: `EMSGSIZE` when more descriptors came than
`max` (the kernel dropped the rest; those that came are closed), `EBUSY` when read_line's buffer holds bytes,
`EOPNOTSUPP` for a TLS connection or one in memory, `io::errc::closed` for a closed one.

## Complexity

One `recvmsg`, linear in the bytes.

## Exceptions

- (1) None but what a blocking wait of the reactor throws (`std::system_error` when its thread cannot be made).
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> receive(net::connection c) {
    vector<byte> buffer(16);
    auto got = co_await c.async_receive_descriptors(buffer);
    io::file end = got->files[0];
    co_await end.async_write("through a passed pipe");
    end.close();
}

int main() {
    net::listener l = net::unix_domain::listen("pipe.sock");
    net::connection a = net::unix_domain::connect("pipe.sock");
    net::connection b = l.accept();
    io::pipe_ends p = io::pipe().value();
    int fds[] = {p.write.fd()};
    a.send_descriptors("pipe", fds).value();
    p.write.close();  // the receiver holds its own copy
    async::run(receive(b));
    println("{}", p.read.read_all_text().value());
    l.close();
}
```

Output:

```text
through a passed pipe
```

## See also

- [send_descriptors](send_descriptors.md): the other side
- [received](../connection-received.md)
- [sgcl::net::connection](README.md)
