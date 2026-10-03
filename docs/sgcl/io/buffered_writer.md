[sgcl](../README.md) › [io](README.md)

# sgcl::io::buffered_writer

```cpp
#include "sgcl/io/buffered.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class buffered_writer;
}
```

`io::buffered_writer` is a writer with a buffer in front of any other, Go's `bufio.Writer`: a write fills a block of
`config::io_buffer_size` (8 KB), and the block is written to the stream underneath when it is full, so that many
small writes cost one write of the stream per 8 KB. [flush](buffered_writer/flush.md) writes what the block holds;
[close](buffered_writer/close.md) flushes and closes the stream underneath.

It is a handle of one word, a `tracked_ptr` to the writer's state (its block, the error it keeps), made by the
constructor over any stream held as an [io::writer](writer.md): `io::buffered_writer out(io::stdout);`, over a
[file](file.md), a [buffer](buffer.md), a connection. Copies share the state. Every error it gives is kept as its
first, and every operation after it gives that error at once and writes nothing, so a buffered writer is written
freely and checked once, at the close: what Go's `bufio.Writer` does with its sticky error. `std` buffers inside an
`std::ostream`'s `streambuf` and reports through its state flags; the destructor of a `std::ofstream` flushes, and
this one's does not.

## Rules

- The destructor does not flush: it runs on the collector's thread, after the sweep that finds the writer dead, and
  could report nothing, so what is not flushed is lost, as with `bufio.Writer`. A buffered writer is flushed or
  closed when done.
- Every error the writer gives is kept as its first: a failure of the stream underneath, or a write after the close.
  Every write and flush after it gives that error at once and writes nothing; `close()` gives it too, after closing
  the stream all the same, and so does every later `close()`. [last_error](buffered_writer/last_error.md) holds it.
  Whoever wants to react earlier checks the result of a single `write` or `flush`.
- A copy is the same writer: one block, one kept error; passed by value into a task, the copy keeps the writer alive
  for as long as the task runs. A handle is a tracked word: on a stack, in a task, in a managed object; in a global
  or a std container, a [rooted](../core/rooted.md) of it, never in a managed object or a task's frame, since a root
  is never part of a cycle.
- A default-constructed writer holds no state (`!w`); an operation on it is a contract violation, asserted in debug
  builds.
- The block is unmanaged memory the writer owns while it is written from the calling thread: nothing hands out a
  slice of it, and a write of it is over when the call returns. The first async operation moves it into a managed
  block, once for the writer's life: a write of a task may run on the blocking pool and outlive the frame of a task
  let go of, so the slice it is given holds the block.
- One thread or task at a time on one writer.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](buffered_writer/buffered_writer.md) | makes a writer over a stream, or an empty handle |
| `(destructor)` | lets go of the state; nothing is flushed |

#### Writing

| Function | Description |
|---|---|
| [write, async_write](buffered_writer/write.md) | writes bytes into the block |
| [flush, async_flush](buffered_writer/flush.md) | writes what the block holds to the stream |

#### Closing

| Function | Description |
|---|---|
| [close, async_close](buffered_writer/close.md) | flushes and closes the stream underneath |
| [is_closed](buffered_writer/is_closed.md) | checks whether the writer was closed |

#### Observers

| Function | Description |
|---|---|
| [buffered](buffered_writer/buffered.md) | the number of bytes in the block, not yet written |
| [available](buffered_writer/available.md) | the room left in the block |
| [last_error](buffered_writer/last_error.md) | the first error given, kept |
| [underlying](buffered_writer/underlying.md) | the stream underneath |
| [operator bool](buffered_writer/operator_bool.md) | checks whether the handle holds a writer |

#### From mixin::writer

The rest of a writer over `write` ([mixin::writer](mixin/writer.md)), brought back beside the writer's own `write`.

| Function | Description |
|---|---|
| [write, async_write](mixin/writer/write.md) | writes text or one byte |
| [copy_from, async_copy_from](mixin/writer/copy_from.md) | writes everything a reader gives, to its end |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](buffered_writer/operator_cmp.md) | checks whether two handles are the same writer |

## Complexity

A write copies its bytes into the block; the stream underneath is written once per 8 KB, and a part of at least a
block's size written to an empty block goes to the stream directly, with no copy.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    io::buffered_writer w(out);
    for (int i : {1, 2, 3}) {
        w.write(to_string(i));
        w.write(byte(' '));
    }
    println("{} buffered, {} written", w.buffered(), out.size());

    if (auto closed = w.close(); !closed) {
        eprintln("{}", closed.error().message());
    }
    println("[{}]", out.text());
}
```

Output:

```text
6 buffered, 0 written
[1 2 3 ]
```

## See also

- [buffered_reader](buffered_reader.md): the same in front of a reader
- [io::writer](writer.md): what the writer is made over
- [print](print.md): formatted text on any writer
- `tests/io/buffered.cpp`: the writer's block, the kept error, the async forms
