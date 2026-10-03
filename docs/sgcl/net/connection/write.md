[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::write, async_write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data) const;                                // (1)
expected<size_t, io::error> write(const string& text) const;                                           // (2)
template<class T>
expected<size_t, io::error> write(const T& text) const;                                                // (3)
async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const noexcept;    // (4)
async::task<expected<size_t, io::error>> async_write(const string& text) const noexcept;               // (5)
template<class T>
async::task<expected<size_t, io::error>> async_write(const T& text) const noexcept;                    // (6)
```

Writes all of the data to the connection, or fails: Go's `Conn.Write`. Two writes at once are taken one after the
other, so that a message written from each of two tasks lands whole; a write and a read run at once.

- (1, 4) The bytes of `data`.
- (2, 5) The bytes of `text`; (5) holds the string for as long as it runs.
- (3, 6) A literal, a character array or a `std::string_view`, as a string: they take part only for those, where
  the conversions to a `string` and to bytes would tie. (6) copies the text into a string the task holds.
- (1–3) On the calling thread, which waits on the [reactor](../../async/readable.md) while the socket is full.
- (4–6) The same for a task, which holds no worker while it waits. (4) writes `data` from where it lies: a slice
  with an owner keeps it alive in the task, a slice of plain memory is the caller's to keep until the task is done.

A write that starts after the write deadline, or would wait past it, fails ([set_write_deadline](set_write_deadline.md)).
A write to a pair in memory waits for the reads of the other end that take it: nothing is buffered.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |
| `text` | the text to write |

## Return value

The number of bytes written, all of them. Or the [io::error](../../io/error/README.md), its operation `write` and its path
the connection:

- `io::errc::closed` when the connection was closed before the call, or while it waited ([close](close.md)), or its
  writing half ended ([close_write](close_write.md)) on a pair in memory;
- `ETIMEDOUT` when the write deadline passed (`is_timeout()`);
- `EPIPE` when the peer closed, never a `SIGPIPE`;
- the `errno` of the socket otherwise (`ECONNRESET`).

A write that fails part way reports the error alone: how many bytes went out before it is lost, as in io, and the
connection is then of no use but to close.

## Complexity

Linear in the bytes written: one system call when the socket takes them at once, and one more per wait for room.

## Exceptions

- (1–3) `std::system_error` when the write has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (4–6) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> show(net::listener l) {
    net::connection c = co_await l.async_accept();
    println("{}", (co_await c.async_read_all_text()).value());
    c.close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(show(l));
    net::connection c = net::tcp::connect(l.local_endpoint());

    vector<byte> bytes = {byte('a'), byte('b'), byte(' ')};
    println("{}", c.write(bytes).value());
    println("{}", c.write(string("string ")).value());
    println("{}", c.write("literal").value());
    c.close_write();
    server.wait();
    c.close();
}
```

Output:

```text
3
7
7
ab string literal
```

## See also

- [read, async_read](read.md): the other direction
- [read_from](read_from.md): a file written to the connection
- [close_write](close_write.md): the end of what this side writes
- [sgcl::net::connection](README.md)
