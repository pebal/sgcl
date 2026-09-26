# sgcl::net::connection, net::listener, net::udp::socket

```cpp
#include "sgcl/net/connection.h"   // or "sgcl/net/net.h"

namespace sgcl::net {
    class connection;   // a stream both ways: TCP, unix, a pair in memory (TLS in stage 2); a handle of one word
    class listener;     // what tcp::listen and unix_domain::listen give: the connections it accepts
    struct datagram;    // a datagram received: its size, its sender, whether it was cut
    class udp::socket;   // what udp::bind and udp::connect give
}
```

The connections of the module. Each is a handle of one word, a `tracked_ptr` to the object inside, as a [`string`](../core/string.md) is: a copy is the same connection, a handle passed into a task by value keeps it alive for as long as the task runs, and an `expected<connection, io::error>` reads `c->read(b)`. They are made by [`tcp`, `udp` and `unix_domain`](socket.md) and by `connection::in_memory()`.

Every operation that may wait comes twice: `read(b)` takes the thread until the data comes, `co_await async_read(b)` gives the worker back meanwhile. Both wait on the [reactor](../async/reactor.md), so a close from any task or thread and a deadline on the module's [clock](../core/clock.md) end either form.

## Rules

- **Full duplex, one at a time per direction.** One read and one write may run at once; two reads at once are taken one after the other (a [`mutex`](../async/mutex.md) of the library per direction, which parks no worker), as are two writes, so that a message written from each of two tasks lands whole.
- **A write writes everything or fails**; a read returns what has come, at least one byte, and 0 at the end of the stream. A write that fails part way (a deadline passing in a long write, a reset) reports the error alone: how many bytes went out before it is lost, as in io, and the stream is then of no use but to close.
- **Deadlines are absolute**, on the module's clock, as Go's: a read (write) that starts after the deadline of its direction, or would wait past it, fails with `ETIMEDOUT` (`is_timeout()`) and takes nothing, even when data is there. `time_point()` removes it. A change applies to the operations in progress: a deadline in the past set from another task ends a read at once. A limit per operation is `c.set_read_deadline(clock::now() + 5s)` before each; `async::timeout(c.read(b), 5s)` is not the same, since the read lost to the timer runs on and takes the data.
- **`close()` from another task cancels**: the reads, writes and accepts in progress end with `io::errc::closed`, and a second close does nothing. The descriptor goes back to the system when the last operation in progress has let go of it, never under one (the race of a close with a read is closed, `io/detail/descriptor.h`). A connection not closed is closed by its destructor, on the collector's thread after the sweep that finds it dead: later than the last use.
- **A write to a peer that closed is `EPIPE`**, never a `SIGPIPE`.
- **Lines** (`read_line`) go through a buffer of 8 KB the first call puts in front of the connection; `read` takes from that buffer first from then on. A line is bounded (64 KB by default, `set_max_line`): the input is the network's.
- A connection is a stream as it is ([stream](../io/stream.md)): `read` and `async_read`, `write` and `async_write`, `close` and `async_close`, so `buffered_reader`, `limit_reader`, `io::copy` and, in the next stage, TLS take it.

## Members

### connection

```cpp
connection() noexcept;                                         // no connection: an operation on it is a contract violation
expected<size_t, io::error> read(const slice<byte>& buffer) const;        // what has come, 0 at the end
async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const;
expected<size_t, io::error> read_full(const slice<byte>& buffer) const;   // the whole buffer, or io::errc::unexpected_eof, the end before the first byte included
async::task<expected<size_t, io::error>> async_read_full(const slice<byte>& buffer) const;
expected<vector<byte>, io::error> read_all() const;                // to the end of the stream
async::task<expected<vector<byte>, io::error>> async_read_all() const;
expected<string, io::error> read_all_text() const;
async::task<expected<string, io::error>> async_read_all_text() const;
expected<optional<string>, io::error> read_line() const;                // without "\n" and "\r\n"; nullopt at the end
async::task<expected<optional<string>, io::error>> async_read_line() const;
void set_max_line(size_t bytes) const;                         // 64 KB by default; longer: io::errc::line_too_long

expected<size_t, io::error> write(const slice<const byte>& data) const;   // everything, or the error
async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const;
expected<size_t, io::error> write(const string& text) const;
async::task<expected<size_t, io::error>> async_write(const string& text) const;   // holds the string while it runs
expected<size_t, io::error> copy_to(const connection& other) const;     // to the end of this stream, written to other; an echo is c.copy_to(c)
async::task<expected<size_t, io::error>> async_copy_to(const connection& other) const;

expected<void, io::error> close() const;  async::task<expected<void, io::error>> async_close() const;   // both ways, now; ends the operations in progress
expected<void, io::error> close_write() const;                          // shutdown(SHUT_WR): the peer reads the end, this side still reads
bool is_closed() const noexcept;

endpoint local_endpoint() const;                               // empty for unix and the pair in memory
endpoint remote_endpoint() const;
string path() const;                                           // a unix socket's path, else empty

void set_deadline(time_point t) const;                         // both directions; time_point() removes it
void set_read_deadline(time_point t) const;
void set_write_deadline(time_point t) const;

expected<void, io::error> set_no_delay(bool on) const;                  // TCP; on by default, as in Go; EOPNOTSUPP for anything else
expected<void, io::error> set_keep_alive(duration idle) const;          // TCP; probes after 15 s by default, as in Go; zero turns them off

static pair<connection, connection> in_memory();               // two connected ends, Go's net.Pipe
explicit operator bool() const noexcept;
friend bool operator==(const connection&, const connection&) noexcept;   // the same connection
```

`in_memory()` gives two ends connected in memory: what one writes the other reads, a write waiting for the reads that take it (nothing is buffered, nothing copied twice), with deadlines, `close` and `close_write` as on a socket. For tests of a protocol without sockets.

```cpp
async::task<> ask(string address) {
    auto c = co_await net::tcp::async_connect(address, 5s);
    if (!c) co_return;
    c->set_deadline(clock::now() + 10s);          // the whole conversation within 10 s
    for (int i : range(3)) {
        co_await c->async_write("PING " + to_string(i) + "\r\n");
        auto line = co_await c->async_read_line();
        if (!line || !*line) break;               // an error, or the end of the stream
        std::cout << **line << '\n';
    }
    co_await c->async_close();
}
```

### listener

```cpp
listener() noexcept;
expected<connection, io::error> accept() const;         // the next connection
async::task<expected<connection, io::error>> async_accept() const;
expected<void, io::error> close() const;                // no more; the accepts in progress end; a unix listener removes its file
bool is_closed() const noexcept;
endpoint local_endpoint() const;               // ":0" given: the port the system chose
string path() const;                           // a unix listener's path
```

`accept` has no deadline of its own: `close()` from another task is how a wait for the next connection is ended (Go's `SetDeadline` on a listener, which a server uses for the same, is not here). It skips a connection aborted before it was taken (`ECONNABORTED`). When the descriptors run out (`EMFILE`, `ENFILE`) it waits, 5 ms and doubling to a second, and tries again rather than spin or fail, as Go's server does; it fails with `io::errc::closed` after `close()`, or on an error that will not pass. (macOS drops the connection it could not take; Linux leaves it in the backlog.) An accepted TCP connection has Nagle off and keep-alive on, as a dialed one.

### udp::socket, udp::datagram

```cpp
struct udp::datagram {
    size_t size = 0;           // the bytes in the buffer
    endpoint from;             // the sender (an IPv4 peer of a dual-stack socket reported as IPv4)
    bool truncated = false;    // the datagram was longer than the buffer, and cut (MSG_TRUNC)
};

expected<udp::datagram, io::error> receive_from(const slice<byte>& buffer) const;
async::task<expected<udp::datagram, io::error>> async_receive_from(const slice<byte>& buffer) const;
expected<size_t, io::error> send_to(const slice<const byte>& data, const endpoint& to) const;
async::task<expected<size_t, io::error>> async_send_to(const slice<const byte>& data, const endpoint& to) const;
expected<size_t, io::error> receive(const slice<byte>& buffer) const;      // a socket from udp::connect: from its one peer
async::task<expected<size_t, io::error>> async_receive(const slice<byte>& buffer) const;
expected<size_t, io::error> send(const slice<const byte>& data) const;     // to its one peer
async::task<expected<size_t, io::error>> async_send(const slice<const byte>& data) const;
expected<void, io::error> close() const;
bool is_closed() const noexcept;
endpoint local_endpoint() const;
endpoint remote_endpoint() const;                               // the peer of a connected socket, else empty
void set_deadline(time_point t) const;
void set_read_deadline(time_point t) const;
void set_write_deadline(time_point t) const;
```

A datagram is sent whole or not at all; `ENOBUFS` (the interface's queue full) is an error, as in Go, not a wait: the socket has room, so a wait for writability would come back at once. An empty buffer takes the next datagram and reports `size` 0, `truncated` when it had bytes (macOS alone would answer with a datagram of nothing and keep the real one queued). An IPv4 address given to `send_to` on a socket bound to both families goes through the mapping.

```cpp
async::task<> udp_echo() {
    auto s = co_await net::udp::async_bind(":5353");
    if (!s) co_return;
    tracked_ptr<array<byte, 8192>> block = make_tracked<array<byte, 8192>>();
    slice<byte> buffer(block, block->data(), block->size());
    while (auto d = co_await s->async_receive_from(buffer)) {
        co_await s->async_send_to(buffer.first(d->size), d->from);
    }
}
```

## See also

- [socket](socket.md): what makes them; [io stream](../io/stream.md), [buffered](../io/buffered.md): what `stream()` plugs into
- [reactor](../async/reactor.md), [clock](../core/clock.md): where the waits wait, and the time of the deadlines
- `tests/net/socket.cpp`, `tests/net/memory.cpp`, `tests/net/race.cpp`
