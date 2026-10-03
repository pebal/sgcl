[sgcl](../../README.md) › [io](../README.md) › [req](../req.md)

# sgcl::io::req::reader, async_reader

```cpp
#include "sgcl/io/req.h"   // or "sgcl/io.h"

namespace sgcl::io::req {
    template<class T>
    concept reader;        // t.read(slice<byte>), or t(slice<byte>), gives
                           // expected<size_t, error> or size_t
    template<class T>
    concept async_reader;  // t.async_read(slice<byte>), or t(slice<byte>),
                           // gives task<expected<size_t, error>>
}
```

`reader` is a stream read on the calling thread: `t.read(b)` with a `slice<byte>` gives something convertible to
`expected<size_t, io::error>`, the number of bytes put at the front of `b`, 0 at the end of the stream, or the
[error](../error.md) of the read. Where `T` has no such method, the object called, `t(b)`, does the same: a lambda
that fills the buffer and says how much is a reader. A plain `size_t` is a stream that does not fail.

`async_reader` is the same for a task: `t.async_read(b)`, or `t(b)`, gives an
`async::task<expected<size_t, io::error>>`, exactly that type, whose `co_await` is what `read` would have returned;
a coroutine lambda is an async reader. The task gives its worker back while it waits, where `read` holds the calling
thread.

`T` is the argument as passed: the type, a reference to it, or a `tracked_ptr`, a `unique_ptr` or a `root_ptr` to
it, looked through. A raw pointer is not a reader. A type that has a `read`, `write`, `async_read` or `async_write`
of any shape is never taken for a callable, so its `operator()`, if any, is not asked. It is Go's `io.Reader`,
asked of the type by the compiler instead of implemented: [read_full](../read_full.md), [read_all](../read_all.md),
[copy](../copy.md) and the other functions of io take every reader, their `async_` forms every async reader, and
[io::reader](../reader.md) holds either as a value.

The two halves are separate requirements: a stream that has only `read` is not an async reader, so
`async_read_full`, `async_read_all` and `async_copy` of it do not compile, rather than hold a thread of the blocking
pool for every wait. [io::reader](../reader.md) is where the missing half is made, when it is wanted: its
`async_read` of a stream that has only `read` runs the read on the [blocking pool](../../async/spawn_blocking.md), and its
`read` of a stream that has only `async_read` waits for the task.

## Satisfied by

- `io::file`, `io::buffer`, `io::buffered_reader`, `io::reader`, `io::limit_reader`, `io::tee_reader`,
  `io::multi_reader`, `io::transform_reader`, the standard streams (`io::stdin`, and `io::stdout` and
  `io::stderr` as well): both;
- `net::connection`, the readers of `compress`, the decoders of `encoding`: both;
- a class with `read(slice<byte>)` (`async_read`) of that shape, a callable of that shape, and a `tracked_ptr` to
  either.

Not by a raw pointer, `io::writer`, or a callable that takes `slice<const byte>`; a stream that has only
`async_read` is not a `reader`, one that has only `read` not an `async_reader`.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A reader of its own; a size_t in place of expected<size_t, io::error> is a
// stream that does not fail
class countdown {
public:
    size_t read(slice<byte> b) {
        if (_n == 0 || b.empty()) {
            return 0;
        }
        b[0] = byte('0' + --_n);
        return 1;
    }

private:
    int _n = 5;
};

int main() {
    countdown c;
    println("{}", *io::read_all_text(c));

    tracked_ptr held = make_tracked<countdown>();
    println("{}", *io::read_all_text(held));  // through the pointer

    // a lambda of the same shape is a reader too
    bool done = false;
    auto once = [&](slice<byte> b) -> size_t {
        if (done) {
            return 0;
        }
        done = true;
        b[0] = byte('!');
        return 1;
    };
    println("{}", *io::read_all_text(once));

    println("{} {} {}", io::req::reader<countdown>, io::req::reader<tracked_ptr<countdown>>,
            io::req::reader<countdown*>);
}
```

Output:

```text
43210
43210
!
true true false
```

A reader for a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <cctype>

using namespace sgcl;

// A reader that upper-cases ASCII on the way through: a class with one
// method, for a task, and no base class (io::transform_reader does the
// same in a line)
class upper_reader {
public:
    explicit upper_reader(const io::reader& r) : _r(r) {}

    async::task<expected<size_t, io::error>> async_read(slice<byte> b) {
        auto n = co_await _r.async_read(b);
        if (n) {
            for (auto& c : b.first(*n)) {
                c = byte(std::toupper(int(c)));
            }
        }
        co_return n;
    }

private:
    io::reader _r;
};

int main() {
    io::buffer greeting("hello, streams\n");
    upper_reader up(greeting);
    auto n = io::async_copy(io::stdout, up).wait();  // a task, waited for by this thread
    if (n) {
        println("{} bytes", *n);
    }
    println("{} {}", io::req::async_reader<upper_reader>, io::req::reader<upper_reader>);
}
```

Output:

```text
HELLO, STREAMS
15 bytes
true false
```

## See also

- [writer, async_writer](writer.md): the other direction
- [io::reader](../reader.md): any reader held as a value, both halves made
- [mixin::reader](../mixin/reader.md): the rest of a reader over `read`
- [sgcl::io::req](../req.md)
