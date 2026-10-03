[sgcl](../../README.md) › [io](../README.md) › [file](../file.md)

# sgcl::io::file::write, async_write

```cpp
/*(1)*/ expected<size_t, error> write(const slice<const byte>& data) const;
/*(2)*/ async::task<expected<size_t, error>> async_write(const slice<const byte>& data)
            const noexcept;
```

Writes the whole of `data` at the position, or at the end for a file opened with `open_flags::append`, and moves
the position past it: the `write(2)` of the descriptor, made again for the rest after a short write and when a
signal interrupts it.

1. On the calling thread. On a non-blocking descriptor (a pipe from [pipe](../pipe.md), a FIFO) a write that would
   block — a full pipe — waits on the [reactor](../../async/readable.md) until there is room, the thread held
   meanwhile.
2. The same for a task, which holds no worker while it waits: a regular file's write runs on the
   [blocking pool](../../async/spawn_blocking.md), a non-blocking descriptor's waits for room on the reactor.

The overloads for a text and for one byte come from [mixin::writer](../mixin/writer/write.md), brought in beside
these: `f.write("text")`, `f.write(s)` for a `string` or a slice of one, `f.write(byte{0})`.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write; a `vector<byte>`, an [array](../../core/array.md), a [buffer](../buffer.md)'s data converts to it |

## Return value

The size of `data`: everything was written. Or the [error](../error.md), its operation `write` and its path the
file's:

- `errc::closed` when the file was closed before the call, or while it waited ([close](close.md));
- `ECANCELED` when the reactor stopped while it waited;
- the `errno` of `write(2)` otherwise (`EBADF` for a file opened only for reading, `ENOSPC` for a full disk,
  `EPIPE` for a pipe or a FIFO whose read end is closed: the write raises no `SIGPIPE`, which would end the
  process).

## Complexity

Linear in the size of `data`: one system call, more after a short write; on a non-blocking descriptor, one more
per wait for room.

## Exceptions

- (1) `std::system_error` when the descriptor is non-blocking, the write has to wait, and the thread of the reactor,
  or of the timers, which its first use starts, cannot be made.
- (2) None: the task's own exceptions are the task's.

## Notes

The data of (2) stays alive while the task awaits, as a task's local does: a `tracked_ptr` to a buffer captured in
the awaiting frame is enough. Given to a regular file without an owner (a plain array, a `std::span`), it is copied
into a managed block before the write starts, since the pool's thread may outlive the frame of a task let go of; a
slice with an owner (a `string`, a `vector`, a [buffer](../buffer.md)) is written as it is.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file f = io::create("out.txt");
    vector<byte> bytes = {byte{'a'}, byte{'b'}, byte{'c'}};
    println("{}", *f.write(bytes));
    println("{}", *f.write(" and a text"));  // the text overload of mixin::writer
    f.close();
    println("{}", *io::read_text("out.txt"));

    auto denied = io::open("out.txt").value().write(bytes);
    println("{}", denied.error().message());
}
```

Output:

```text
3
11
abc and a text
write out.txt: Bad file descriptor
```

A task writes a regular file on the pool and a pipe on the reactor:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> fill(io::file f) {
    string text = "from a task";
    auto n = co_await f.async_write(text);
    println("{}: {}", f.path(), *n);
    f.close();
}

int main() {
    async::run(fill(io::create("task.txt")));
    auto [in, out] = io::pipe().value();
    async::run(fill(out));
    println("{}, {}", *io::read_text("task.txt"), *in.read_all_text());
}
```

Output:

```text
task.txt: 11
pipe: 11
from a task, from a task
```

## See also

- [read, async_read](read.md): the other direction
- [write_at, async_write_at](write_at.md): a write at an offset, the position untouched
- [write](../mixin/writer/write.md): a text or a byte
- [write_file](../write_file.md): a whole file in one call
- [buffered_writer](../buffered_writer.md): small writes gathered into one
- [sgcl::io::file](../file.md)
