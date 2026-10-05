[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::writer

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class writer;   // : public mixin::writer<writer>
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::writer` is any writer, held as a value: whatever meets [req::writer](../req/writer.md) or
[req::async_writer](../req/writer.md) — a handle of the library (a [file](../file/README.md), a [buffer](../buffer/README.md), a
[buffered_writer](../buffered_writer/README.md), a `net::connection`), a standard stream, a stream of the program's, a lambda
that takes the bytes — kept where a stream must be kept: the output of an encoder, the destination of a log, the
standard output of a child process. It is the counterpart of [reader](../reader/README.md), three words: a `tracked_ptr` of
the object that keeps the stream, a pointer to the stream and a pointer to a table of its methods, made once per
type. That is Go's `io.Writer` interface value, with `close` as Go's `io.WriteCloser` has it. The standard library
has no counterpart: a `std::ostream` is a class of a hierarchy over a virtual `std::streambuf`, where the stream
here declares nothing virtual and derives from nothing ([req](../req/README.md)).

What the writer keeps depends on how the stream is given, as for a reader: a handle of the library is held by its
object, so the handle may go first; a stream given by reference is referenced, with the managed object it lies in
kept, if it lies in one (one on a stack, a global such as `io::stdout`, or one a `unique_ptr` owns is the caller's
to keep alive); a `tracked_ptr` holds it; a callable or a temporary is copied into a managed object of its own.

The half a stream lacks is made from the other, and only here: [write](write.md) of a stream that has only
`async_write` waits for its task on the calling thread, `async_write` of a stream that has only `write` runs it on
the [blocking pool](../../async/spawn_blocking.md). [has_write](has_write.md) and
[has_async_write](has_async_write.md) tell which halves are the stream's own. A write writes all of the data
or fails, as Go's `Write` does; a callable that returns nothing is a writer that does not fail.

## Rules

- A copy is the same stream: the three words copied, the stream shared.
- One thread or task at a time on one stream, as with the stream itself.
- An async write that runs on the blocking pool (the async half made of a stream's `write`) and is given a slice
  without an owner goes through a managed block: the data is copied into it before the operation starts, while the
  caller waits. The pool's thread may outlive the frame of a task let go of, and it never touches plain memory that
  died with it. A slice with an owner (a `string`, a `vector`, a `buffer`, any type of the library) is used as it
  is, with no copy.
- A stream's destructor runs on the collector's thread, after the sweep that finds the object dead: a stream that
  holds a descriptor is closed by [close](close.md) when done, which releases it now and reports the error a
  deferred close cannot.
- Errors are values, `expected<T, error>` ([error](../error/README.md)).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](writer.md) | constructs the writer, empty or holding a stream |
| `(destructor)` | drops the words; the stream is not closed |
| `operator=` | copies or moves the three words of another writer |

#### Operations

| Function | Description |
|---|---|
| [write, async_write](write.md) | writes bytes to the stream |
| [close, async_close](close.md) | closes the stream, when it has a close |

#### Observers

| Function | Description |
|---|---|
| [has_write](has_write.md) | checks whether `write` is the stream's own |
| [has_async_write](has_async_write.md) | checks whether `async_write` is the stream's own |
| [has_close](has_close.md) | checks whether the stream has a close |
| [fd](fd.md) | the descriptor under the stream, -1 for none |
| [operator bool](operator_bool.md) | checks whether the writer holds a stream |

#### From mixin::writer

The rest of what a writer does, each an algorithm of io over this stream ([mixin::writer](../mixin/writer/README.md)).

| Function | Description |
|---|---|
| [write, async_write](../mixin/writer/write.md) | writes a text or one byte |
| [copy_from, async_copy_from](../mixin/writer/copy_from.md) | everything from a reader to its end, written here |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | checks whether two writers hold the same stream |

## Complexity

A copy: three words. A call: one indirect call through the table, to the stream's own method.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A log whose destination is chosen when it is made: any stream
struct Log {
    io::writer out;

    void line(const string& text) {
        out.write(text);
        out.write(byte('\n'));
    }
};

int main() {
    io::buffer kept;
    Log to_memory{kept};
    Log to_terminal{io::stdout};

    size_t counted = 0;
    Log to_counter{[&](slice<const byte> data) { counted += data.size(); }};  // cannot fail

    to_memory.line("started");
    to_terminal.line("started");
    to_counter.line("started");
    println("{} bytes kept, {} counted", kept.size(), counted);
}
```

Output:

```text
started
8 bytes kept, 8 counted
```

## See also

- [reader](../reader/README.md): any reader, as a value
- [req::writer](../req/writer.md), [req::async_writer](../req/writer.md): what a stream is
- [mixin::writer](../mixin/writer/README.md): the methods a writer has beside `write`
- [buffered_writer](../buffered_writer/README.md): a block in front of any writer
- [multi_writer](../multi_writer/README.md), [discard_writer](../discard_writer/README.md): writers of the module
