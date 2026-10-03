[sgcl](../../README.md) › [io](../README.md) › [req](README.md)

# sgcl::io::req::closer, async_closer

```cpp
#include "sgcl/io/req.h"   // or "sgcl/io.h"

namespace sgcl::io::req {
    template<class T>
    concept closer;        // t.close() gives expected<void, error>
    template<class T>
    concept async_closer;  // t.async_close() gives task<expected<void, error>>
}
```

`closer` is a stream with something to release: `t.close()` gives something convertible to
`expected<void, io::error>`, success or the [error](../error/README.md) of the close. `async_closer` is a stream whose close
may wait, closed by a task: `t.async_close()` gives an `async::task<expected<void, io::error>>`, exactly that type.
`T` is the argument as passed: the type, a reference to it, or a `tracked_ptr`, a `unique_ptr` or a `root_ptr` to
it, looked through. A close is a method; a callable is never a closer.

It is Go's `io.Closer`. The destructor of a stream of the library runs on the collector's thread, after the sweep
that finds it dead, which may be long after its last use; a stream that is done is closed, which releases what it
holds now and reports the error a deferred close cannot. A stream held as [io::reader](../reader/README.md) or
[io::writer](../writer/README.md) is closed through it, Go's `ReadCloser` and `WriteCloser`: the handle's `close()` is the
stream's when it is a closer (`has_close()`), and waits for the task of one that is only an async closer; its
`async_close()` is the stream's when it is an async closer, and runs the `close()` of one that is only a closer (a
`file`, whose close does not wait) on the [blocking pool](../../async/spawn_blocking.md); of a stream that has neither,
nothing is closed and both succeed. [buffered_reader](../buffered_reader/README.md) and
[buffered_writer](../buffered_writer/README.md) close the stream under them the same way.

## Satisfied by

- `io::reader`, `io::writer`, `io::buffered_reader`, `io::buffered_writer`, `net::connection`, the readers and
  writers of `compress`, the encoders of `encoding`: both;
- `io::file`: `closer`;
- a class with `close()` (`async_close()`) of that shape, and a `tracked_ptr` to one.

Not by `io::buffer`, `io::discard`, `io::limit_reader`, the decoders of `encoding` or the standard streams
(`io::stdout` never closes its descriptor), nor by a lambda; `io::file` is not an `async_closer`.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A writer with a close of its own
class sink {
public:
    size_t write(slice<const byte> b) {
        return b.size();
    }

    expected<void, io::error> close() {
        println("sink closed");
        return {};
    }
};

int main() {
    sink s;
    io::writer w(s);
    println("{}", w.has_close());
    w.close();  // the stream's own close, through the handle

    io::writer d(io::discard);
    println("{} {}", d.has_close(), bool(d.close()));  // nothing to close: success
    println("{} {}", io::req::closer<sink>, io::req::closer<io::buffer>);
}
```

Output:

```text
true
sink closed
false true
true false
```

A close for a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    io::buffered_writer w(out);
    w.write("kept in the block until the close");
    println("{}", out.size());

    auto closed = w.async_close().wait();  // the block written for a task, then out's close: none
    println("{} {}", bool(closed), out.text());
    println("{} {}", io::req::async_closer<io::buffered_writer>, io::req::async_closer<io::file>);
}
```

Output:

```text
0
true kept in the block until the close
true false
```

## See also

- [io::reader](../reader/README.md), [io::writer](../writer/README.md): any stream held as a value, closed through it
- [file::close](../file/close.md): a descriptor given back
- [sgcl::io::req](README.md)
