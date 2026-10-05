[sgcl](../../README.md) › [net](../README.md)

# sgcl::net::connection

```cpp
#include "sgcl/net/connection.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class connection;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::connection` is a stream of bytes both ways: a TCP socket, a unix socket, a TLS session
([net::tls](../tls/README.md)), or one end of a pair in memory ([in_memory](in_memory.md)). It is Go's
`net.Conn` with its `Read`, `Write`, `Close`, `CloseWrite`, `LocalAddr` and `RemoteAddr`
([read](read.md), [write](write.md), [close](close.md),
[close_write](close_write.md), [local_endpoint](local_endpoint.md),
[remote_endpoint](remote_endpoint.md)), its deadlines and its socket options, and what Go takes from
`io` and `bufio` for it: `io.ReadFull`, `io.ReadAll` and `io.Copy(c, c)` are [read_full](read_full.md),
[read_all](read_all.md) and [copy_to](copy_to.md), `bufio.NewReader(c).ReadString('\n')` is
[read_line](read_line.md).

A connection is a handle of one word, a `tracked_ptr` to the object inside, as a [string](../../core/string/README.md) is: a
copy is the same connection, as a `*net.TCPConn` is in Go, and a handle passed by value into a task keeps the
connection alive for as long as the task runs. An `expected<connection, io::error>` reads `c->read(b)`, and
`c.read(b)` once unwrapped. It is a stream as it is, as a `net.Conn` is an `io.ReadWriteCloser`: `read` and
`async_read`, `write` and `async_write`, `close` and `async_close` make it an [io::reader](../../io/reader/README.md) and an
[io::writer](../../io/writer/README.md), so [buffered_reader](../../io/buffered_reader/README.md), [limit_reader](../../io/limit_reader/README.md)
and [io::copy](../../io/copy.md) take it, and a connection [net::tls](../tls/README.md) makes is one too. A stream made of
a connection (`io::reader in = c;`) holds the connection itself, not the handle, which may go first.

Every operation that may wait comes twice: `read(b)` takes the thread until the data comes, `co_await
async_read(b)` gives the worker back meanwhile. Both wait on the [reactor](../../async/readable.md), so a close from any
task or thread, and a deadline on the module's [clock](../../core/clock/README.md) (a `manual_clock` included), end either
form: the blocking form parks the thread in the descriptor's slot on the reactor rather than polling the
descriptor, which neither a close nor the manual clock could interrupt.

## Rules

- Made by [tcp::connect](../tcp/connect.md), [unix_domain::connect](../unix_domain/connect.md),
  [listener::accept](../listener/accept.md), [tls::connect](../tls/connect.md), [tls::client](../tls/client.md),
  [tls::server](../tls/server.md) and [in_memory](in_memory.md). A connection made by the default
  constructor holds none (`!c`); an operation on it is a contract violation (debug builds assert).
- A connection takes part in the atomics by its word ([atomic](../../core/atomic-handle/README.md)), compared by
  identity.
- Full duplex, one at a time per direction. One read and one write may run at once; two reads at once are taken one
  after the other, by a [mutex](../../async/mutex/README.md) of the library per direction which parks no worker, and so are
  two writes, so that a message written from each of two tasks lands whole.
- A write writes everything or fails; a read returns what has come, at least one byte, and 0 at the end of the
  stream. A write that fails part way (a deadline passing in a long write, a reset) reports the error alone: how
  many bytes went out before it is lost, as in io, and the stream is then of no use but to close.
- The deadlines are absolute, on the module's clock, as Go's: a read (a write) that starts after the deadline of its
  direction, or would wait past it, fails with `ETIMEDOUT` (`is_timeout()`) and takes nothing, even when data is
  there ([set_deadline](set_deadline.md)).
- [close](close.md) from another task cancels: the reads, writes and accepts in progress end with
  `io::errc::closed`. The descriptor goes back to the system when the last operation in progress has let go of
  it, never under one. A connection not closed is closed by its destructor, on the collector's thread after the
  sweep that finds it dead: later than the last use.
- A write to a peer that closed is `EPIPE`, never a `SIGPIPE` (`SO_NOSIGPIPE`, `MSG_NOSIGNAL`). Every socket is
  non-blocking and close-on-exec.
- [read_line](read_line.md) puts a buffer of 8 KB in front of the connection at its first call, and
  `read` takes from that buffer first from then on. A line is bounded, 64 KB by default
  ([set_max_line](set_max_line.md)): the input is the network's.
- Errors are values, [expected\<T, io::error\>](../../io/error/README.md): the code (an `errno` value, `io::errc::closed`, or
  one of [net::errc](../errc.md)), the operation and the connection, so that `message()` reads `read tcp
  127.0.0.1:50000->127.0.0.1:8080: Operation timed out`, a unix socket named by its path and a pair in memory by
  `pipe`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](connection.md) | constructs the handle: no connection, or a copy that is the same connection |
| `(destructor)` | releases the handle; the socket is closed by `close`, or when the collector finds the connection dead |
| `operator=` | makes the handle the same connection as another |

#### Reading

| Function | Description |
|---|---|
| [read, async_read](read.md) | reads what has come, at least one byte |
| [read_full, async_read_full](read_full.md) | fills the whole buffer, or says why not |
| [read_all, async_read_all](read_all.md) | reads to the end of the stream, into a `vector<byte>` |
| [read_all_text, async_read_all_text](read_all_text.md) | reads to the end of the stream, into a `string` |
| [read_line, async_read_line](read_line.md) | reads the next line |
| [set_max_line](set_max_line.md) | sets the longest line `read_line` takes |
| [max_line](max_line.md) | the longest line `read_line` takes |

#### Writing

| Function | Description |
|---|---|
| [write, async_write](write.md) | writes all of the data or fails |
| [read_from, async_read_from](read_from.md) | sends a file from its position to its end (`sendfile` over TCP) |
| [copy_to, async_copy_to](copy_to.md) | copies everything to the end of this stream into another connection |

#### Closing

| Function | Description |
|---|---|
| [close, async_close](close.md) | ends the connection both ways, and the operations in progress |
| [close_write](close_write.md) | ends the writing half (`shutdown(SHUT_WR)`) |
| [is_closed](is_closed.md) | checks whether the connection was closed |

#### Deadlines

| Function | Description |
|---|---|
| [set_deadline](set_deadline.md) | sets the deadline of both directions |
| [set_read_deadline](set_read_deadline.md) | sets the deadline of the reads |
| [set_write_deadline](set_write_deadline.md) | sets the deadline of the writes |
| [read_deadline](read_deadline.md) | the deadline of the reads |
| [write_deadline](write_deadline.md) | the deadline of the writes |

#### Socket options

| Function | Description |
|---|---|
| [set_no_delay](set_no_delay.md) | turns Nagle's algorithm off or on (TCP) |
| [set_keep_alive](set_keep_alive.md) | sets the silence after which keep-alive probes go out (TCP) |

#### Observers

| Function | Description |
|---|---|
| [local_endpoint](local_endpoint.md) | the address of this end |
| [remote_endpoint](remote_endpoint.md) | the address of the peer |
| [path](path.md) | the path of a unix socket |
| [operator bool](operator_bool.md) | checks whether the handle holds a connection |

#### In memory

| Function | Description |
|---|---|
| [in_memory](in_memory.md) | makes two ends connected in memory, Go's `net.Pipe` (static) |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same connection |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

// Answers each line with PONG until the client ends its half
async::task<> answer(net::listener l) {
    net::connection c = co_await l.async_accept();
    for (;;) {
        auto line = co_await c.async_read_line();
        if (!line || !*line) {
            break;  // an error, or the end of the stream
        }
        co_await c.async_write("PONG " + **line + "\n");
    }
    co_await c.async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(answer(l));

    net::connection c = net::tcp::connect(l.local_endpoint());
    c.set_deadline(clock::now() + 10s);  // the whole conversation within 10 s
    for (int i : range(3)) {
        c.write("PING " + to_string(i) + "\n");
        println("{}", c.read_line()->value());
    }
    c.close_write();
    server.wait();
    c.close();
    l.close();
}
```

Output:

```text
PONG PING 0
PONG PING 1
PONG PING 2
```

## See also

- [tcp](../tcp/README.md), [unix_domain](../unix_domain/README.md), [listener](../listener/README.md): what makes a connection
- [io::reader](../../io/reader/README.md), [io::writer](../../io/writer/README.md), [buffered_reader](../../io/buffered_reader/README.md): what a
  connection plugs into
- [reactor](../../async/readable.md), [clock](../../core/clock/README.md): where the waits wait, and the time of the deadlines
- [io::error](../../io/error/README.md), [errc](../errc.md): what a failure says
- `tests/net/socket.cpp`, `tests/net/memory.cpp`, `tests/net/race.cpp`
