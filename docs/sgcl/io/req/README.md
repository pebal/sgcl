[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::req

```cpp
#include "sgcl/io/req.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class seek_from { begin, current, end };

    namespace req {
        template<class T> concept reader;
        template<class T> concept async_reader;
        template<class T> concept writer;
        template<class T> concept async_writer;
        template<class T> concept closer;
        template<class T> concept async_closer;
        template<class T> concept seeker;
    }
}
```

The requirements of io (`namespace sgcl::io::req`) say what the module takes as a stream: whatever has the
primitive. A reader has `read(slice<byte>)`, which returns the bytes read, 0 at the end of the stream; a writer has
`write(slice<const byte>)`, which writes all of it, as Go's `Write`, or fails with an error that says how far it got.
Both do their work now, on the calling thread: io is synchronous, as `read` and `write` are in POSIX, in `std` and in
Go. A stream that can wait without holding a thread (a socket, a pipe on the reactor) has `async_read` and
`async_write` as well, which return a task for `co_await`. A stream that releases something has `close()`, one that
moves its position `seek(offset, from)`.

No base class and nothing virtual on the stream's side: a class of your own is a reader by having `read`, and a
lambda that fills a buffer and says how much is a reader too, as one that takes the bytes is a writer. The concepts
are checked where a stream is passed, so a function of io that needs a half the stream does not have does not
compile: [async_copy](../copy.md) of a source that has only `read` is refused at the call, rather than hold a thread of
the pool for every wait. Go's `io.Reader`, `io.Writer`, `io.Closer` and `io.Seeker` are interfaces a type implements;
here they are concepts over the methods, and [reader](../reader/README.md) and [writer](../writer/README.md) hold any stream as a value
where one has to be kept, Go's interface value.

## Rules

- The argument is taken as it is passed: a type, a reference to it, or what owns it and is dereferenced — a
  `tracked_ptr`, a `unique_ptr` from `make_tracked`, a `root_ptr` (anything with `get()`, `->` and `*`) — and the
  concept looks through the pointer. A raw pointer is never a stream: an object of the program is given by
  reference when it lies on a stack or in a global, by its `tracked_ptr` when it lies on the managed heap.
- A primitive is a method of the name or the object itself called (a lambda, a class with `operator()`), of the same
  shape. A method wins where a type has both, and a type with a `read`, `write`, `async_read` or `async_write` of
  any shape is never taken for a callable.
- A plain `size_t` in place of `expected<size_t, io::error>` is a stream that does not fail; a callable writer may
  also return nothing, which writes all it is given.
- The blocking half and the task's half are separate requirements: a stream with `read` alone is not an
  `async_reader`. [reader](../reader/README.md) and [writer](../writer/README.md) make the half a stream lacks, and only they.
- The functions of io take every stream that meets the requirement, and the classes of the library carry the same
  functions as members, from io's [mixins](../mixin/README.md).

## Requirements

| Requirement | Description |
|---|---|
| [closer, async_closer](closer.md) | a stream with `close()`; with `async_close()`, a task |
| [reader, async_reader](reader.md) | a stream read on the calling thread, `read(slice<byte>)`; read by a task, `async_read` |
| [seeker](seeker.md) | a stream with a position: `seek(offset, from)` |
| [writer, async_writer](writer.md) | a stream written on the calling thread, `write(slice<const byte>)`; by a task, `async_write` |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// Any stream that has read, the program's own or the library's
size_t count_bytes(io::req::reader auto&& r) {
    auto all = io::read_all(r);
    return all ? all->size() : 0;
}

// A reader of its own: a method of the shape, no base class
class zeros {
public:
    size_t read(slice<byte> b) {
        size_t n = b.size() < _left ? b.size() : _left;
        b.first(n).fill(byte(0));
        _left -= n;
        return n;
    }

private:
    size_t _left = 100000;
};

int main() {
    zeros z;
    io::buffer text("hello");
    println("{} {}", count_bytes(z), count_bytes(text));
    println("{} {}", io::req::reader<zeros>, io::req::async_reader<zeros>);
    println("{} {}", io::req::writer<io::buffer>, io::req::seeker<io::buffer>);
}
```

Output:

```text
100000 5
true false
true true
```

## See also

- [the mixins](../mixin/README.md): the rest of a reader, a writer and a seeker over the primitive
- [reader](../reader/README.md), [writer](../writer/README.md): any stream held as a value
- [copy](../copy.md), [read_all](../read_all.md), [write](../write.md): the functions over any stream
- [seek_from](../seek_from.md): where a seek counts from
- `tests/io/stream.cpp`
