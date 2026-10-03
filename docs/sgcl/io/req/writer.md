[sgcl](../../README.md) › [io](../README.md) › [req](README.md)

# sgcl::io::req::writer, async_writer

```cpp
#include "sgcl/io/req.h"   // or "sgcl/io.h"

namespace sgcl::io::req {
    template<class T>
    concept writer;        // t.write(slice<const byte>), or t(slice<const byte>), gives
                           // expected<size_t, error>, size_t, or (a callable) nothing
    template<class T>
    concept async_writer;  // t.async_write(slice<const byte>), or t(slice<const byte>),
                           // gives task<expected<size_t, error>>
}
```

`writer` is a stream written on the calling thread: `t.write(b)` with a `slice<const byte>` gives something
convertible to `expected<size_t, io::error>`. A write writes all of `b`, and gives its size, or fails with an
[error](../error/README.md) that says how far it got, as Go's `Write`. Where `T` has no such method, the object called,
`t(b)`, does the same, and a callable may also return nothing: a lambda that takes the bytes is a writer that
writes all it is given and does not fail. A plain `size_t` is a stream that does not fail either.

`async_writer` is the same for a task: `t.async_write(b)`, or `t(b)`, gives an
`async::task<expected<size_t, io::error>>`, exactly that type, whose `co_await` is what `write` would have returned.
The task gives its worker back while it waits, where `write` holds the calling thread.

`T` is the argument as passed: the type, a reference to it, or a `tracked_ptr`, a `unique_ptr` or a `root_ptr` to
it, looked through; never a raw pointer. A type that has a `read`, `write`, `async_read` or `async_write` of any
shape is never taken for a callable. It is Go's `io.Writer`, asked of the type by the compiler:
[write](../write.md) of text or bytes and [copy](../copy.md) take every writer, their `async_` forms every async
writer, and [io::writer](../writer/README.md) holds either as a value. The two halves are separate requirements: a stream
that has only `write` is not an async writer, and `async_write` and `async_copy` into it do not compile;
[io::writer](../writer/README.md) makes the missing half when it is wanted, its `async_write` of a stream that has only
`write` running the write on the [blocking pool](../../async/spawn_blocking.md).

## Satisfied by

- `io::file`, `io::buffer`, `io::buffered_writer`, `io::writer`, `io::multi_writer`, `io::discard`, the standard
  streams (`io::stdout`, `io::stderr`, and `io::stdin` as well): both;
- `net::connection`, the writers of `compress`, the encoders of `encoding`: both;
- a class with `write(slice<const byte>)` (`async_write`) of that shape, a callable of that shape, and a
  `tracked_ptr` to either.

Not by a raw pointer or `io::reader`; a stream that has only `async_write` is not a `writer`, one that has only
`write` not an `async_writer`.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A writer of its own: it counts what it is given and keeps nothing
class counter {
public:
    size_t write(slice<const byte> b) {
        total += b.size();
        return b.size();
    }

    size_t total = 0;
};

int main() {
    counter c;
    io::write(c, "hello, ");
    io::write(c, "world");
    println("{}", c.total);

    // a lambda that returns nothing is a writer that does not fail
    size_t lines = 0;
    auto newlines = [&](slice<const byte> b) {
        lines += b.count_of([](byte x) { return x == byte('\n'); });
    };
    io::copy(newlines, io::buffer("one\ntwo\nthree\n"));
    println("{}", lines);

    println("{} {}", io::req::writer<counter>, io::req::writer<counter*>);
}
```

Output:

```text
12
3
true false
```

A writer for a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

// A writer for a task: it counts what it is given and keeps nothing
class task_counter {
public:
    async::task<expected<size_t, io::error>> async_write(slice<const byte> b) {
        total += b.size();
        co_return b.size();
    }

    size_t total = 0;
};

int main() {
    task_counter c;
    auto n = io::async_copy(c, io::buffer("twelve bytes")).wait();
    println("{} {}", *n, c.total);
    println("{} {}", io::req::async_writer<task_counter>, io::req::writer<task_counter>);
}
```

Output:

```text
12 12
true false
```

## See also

- [reader, async_reader](reader.md): the other direction
- [io::writer](../writer/README.md): any writer held as a value, both halves made
- [mixin::writer](../mixin/writer/README.md): the rest of a writer over `write`
- [sgcl::io::req](README.md)
