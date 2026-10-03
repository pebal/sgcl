[sgcl](../README.md) › [io](README.md)

# sgcl::io::file

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class file final : public mixin::reader<file>, public mixin::writer<file>,
                       public mixin::seeker<file>;
}
```

`sgcl::io::file` is one class for every descriptor — a regular file, a pipe, a terminal, later a socket — a stream
with a position. A read or a write is the system call on the descriptor. The asynchronous form of an operation goes
one of two ways, chosen when the file is made: a regular file's (and a terminal's) runs the call on the
[blocking pool](../async/spawn_blocking.md), a disk having no readiness to wait for; a non-blocking descriptor's (a pipe
from [pipe](pipe.md), a socket) waits for readiness on the [reactor](../async/readable.md) and makes the call when it
will not block, and the synchronous form on such a descriptor polls instead. [read_at](file/read_at.md) and
[write_at](file/write_at.md) are `pread` and `pwrite`: the position given, the file's own untouched, so that tasks
share a file without a seek between them.

A `file` is a handle of one word, a `tracked_ptr` to the object inside, as a [string](../core/string.md) is: a copy
is the same file (one descriptor, one position), and passed by value into a task it keeps the file alive for as
long as the task runs. `io::file f = io::open(p);` takes the file out of the `expected`, and throws its error when
there is none ([expected](../core/expected.md)); `auto opened = io::open(p); if (!opened) ...` is the form that
looks at the failure.

Against `std::fstream`: no buffer of its own (a [buffered_reader](buffered_reader.md) or a
[buffered_writer](buffered_writer.md) goes over it), no formatting, the errors returned as values, a copy that
shares the file instead of none, and an `async_` form, for a task, of the operations that wait. Against Go's
`os.File`: the same class for every descriptor and the same operations (`ReadAt`, `WriteAt`, `Seek`, `Sync`,
`Truncate`, `Stat`, `Chmod` are [read_at](file/read_at.md), [write_at](file/write_at.md), [seek](file/seek.md),
[sync](file/sync.md), [truncate](file/truncate.md), [stat](file/stat.md), [chmod](file/chmod.md)); `os.Open`,
`os.Create`, `os.OpenFile` and `os.NewFile` are [open](open.md) with its flags, [create](create.md) and
[from_fd](from_fd.md). Where a goroutine blocks in `Read`, a task writes `co_await f.async_read(b)`, served by the
pool for a regular file and by the reactor for a pipe or a socket.

## Rules

- Made by [open](open.md), [create](create.md), [temp_file](temp_file.md), [from_fd](from_fd.md),
  [pipe](pipe.md) and their `async_` forms: a handle, which every function of io takes as a reader, a writer, a
  closer and a seeker ([req::reader](req/reader.md), [req::writer](req/writer.md), [req::closer](req/closer.md),
  [req::seeker](req/seeker.md)). A `file` made by its default constructor holds none (`!f`); an operation on it is
  a contract violation.
- A handle is a tracked word: on a stack, in a task, in a managed object. In a global or a `std` container it goes
  into a [rooted](../core/rooted.md): `rooted<io::file> log(io::open(p));`, then `log->write(...)` — a copy of the
  handle in a managed object of its own under a root, the same file. A root is never part of a cycle: a `rooted`
  never lies in a managed object or in a task's frame, where the handle itself goes.
- A stream made of a file (`io::reader in = f;`, [reader](reader.md), [writer](writer.md)) holds the file itself,
  not the handle: the handle may go first.
- [close](file/close.md) from any thread or task ends the file: no operation starts after it, and an operation
  waiting in `read` or `write` wakes to `errc::closed`. The descriptor is released by `close()` or, failing that,
  by the destructor on the collector's thread after the sweep that finds the file dead — later than the last use.
  A file that is done is closed; `close()` reports what the deferred one could not.
- [from_fd](from_fd.md) leaves the descriptor's flags as they are: one that is non-blocking already is served by the
  reactor, any other by the pool. [open](open.md) makes a FIFO or a device non-blocking; [pipe](pipe.md) makes both
  ends so.
- An async read or write that runs on the blocking pool (a regular file, `read_at`, `write_at`) and is given a
  slice without an owner goes through a managed block: the data to write is copied into it before the operation
  starts, the bytes read are copied back from it when the task resumes. The pool's thread may go on after the frame
  of a task let go of, and it never touches plain memory that died with that frame. A slice with an owner (a
  `string`, a `vector`, a [buffer](buffer.md), any type of the library) is used as it is, with no copy: give one.
- One task or thread at a time on the position; `read_at` and `write_at` from any number.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](file/file.md) | constructs the handle: no file, or a copy that is the same file |
| `(destructor)` | releases the handle; the descriptor is closed by `close`, or when the collector finds the file dead |
| [operator=](file/operator_assign.md) | makes the handle the same file as another |

#### Reading and writing

| Function | Description |
|---|---|
| [read, async_read](file/read.md) | reads what is available at the position |
| [write, async_write](file/write.md) | writes the whole of the data at the position |
| [read_at, async_read_at](file/read_at.md) | reads at an offset, the position untouched (`pread`) |
| [write_at, async_write_at](file/write_at.md) | writes at an offset, the position untouched (`pwrite`) |

#### Positioning

| Function | Description |
|---|---|
| [seek](file/seek.md) | moves the position (`lseek`) |

#### File operations

| Function | Description |
|---|---|
| [close](file/close.md) | ends the file and gives the descriptor back |
| [is_closed](file/is_closed.md) | checks whether the file was closed |
| [sync, async_sync](file/sync.md) | waits until what was written reaches the disk (`fsync`) |
| [truncate, async_truncate](file/truncate.md) | changes the size of the file (`ftruncate`) |
| [stat](file/stat.md) | what is known of the file (`fstat`) |
| [chmod, async_chmod](file/chmod.md) | changes the permissions of the file (`fchmod`) |

#### Observers

| Function | Description |
|---|---|
| [fd](file/fd.md) | the descriptor, `-1` when closed |
| [path](file/path.md) | the path the file was opened with, or the name given to `from_fd` |
| [is_nonblocking](file/is_nonblocking.md) | checks whether the async operations wait on the reactor |
| [operator bool](file/operator_bool.md) | checks whether the handle holds a file |

#### From mixin::reader

[mixin::reader](mixin/reader.md): the algorithms of io over this file, each with its `async_` form.

| Function | Description |
|---|---|
| [read_full, async_read_full](mixin/reader/read_full.md) | fills the whole buffer, or says why not |
| [read_all, async_read_all](mixin/reader/read_all.md) | reads to the end, into a `vector<byte>` |
| [read_all_text, async_read_all_text](mixin/reader/read_all_text.md) | reads to the end, into a `string` |
| [copy_to, async_copy_to](mixin/reader/copy_to.md) | copies everything to the end into a writer |

#### From mixin::writer

[mixin::writer](mixin/writer.md): the overloads of `write` and `async_write` beside the file's own.

| Function | Description |
|---|---|
| [write, async_write](mixin/writer/write.md) | writes a text (a `string`, a slice of one, a literal, a `std::string_view`) or one byte |
| [copy_from, async_copy_from](mixin/writer/copy_from.md) | copies everything from a reader to its end into the file |

#### From mixin::seeker

[mixin::seeker](mixin/seeker.md), over [seek](file/seek.md).

| Function | Description |
|---|---|
| [tell](mixin/seeker/tell.md) | the position |
| [size](mixin/seeker/size.md) | the size, the position kept |
| [rewind](mixin/seeker/rewind.md) | moves the position to the beginning |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](file/operator_cmp.md) | checks whether two handles are the same file |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

// Copies a file through the pool without holding a worker, then reads the copy back through a
// pipe; the failures returned, not thrown. By value: a task copies its parameters into its frame
async::task<expected<size_t, io::error>> roundtrip(string src, string dst) {
    auto in = co_await io::async_open(src);
    if (!in) co_return unexpected(in.error());
    auto out = co_await io::async_create(dst);
    if (!out) co_return unexpected(out.error());
    auto n = co_await in->async_copy_to(*out);  // reads and writes on the blocking pool
    if (!n) co_return n;
    out->close();  // a file's close never waits

    auto ends = io::pipe();  // a pipe: both ends on the reactor
    if (!ends) co_return unexpected(ends.error());
    async::go([sending = ends->write, dst]() -> async::task<> {
        auto data = co_await io::async_read_file(dst);
        if (data) co_await sending.async_write(data);
        sending.close();  // the reader sees the end
    });
    auto back = co_await ends->read.async_read_all();  // suspended until the writer is done
    if (!back) co_return unexpected(back.error());
    co_return back->size();
}

int main() {
    io::write_file("notes.txt", "one\ntwo\nthree\n");
    auto n = async::run(roundtrip("notes.txt", "notes.copy"));
    if (!n) {
        eprintln(n.error().message());
        return 1;
    }
    println("{} bytes", *n);
    println("{}", async::run(roundtrip("missing.txt", "x")).error().message());
}
```

Output:

```text
14 bytes
open missing.txt: No such file or directory
```

## See also

- [open](open.md), [create](create.md), [pipe](pipe.md), [from_fd](from_fd.md), [temp_file](temp_file.md): what
  makes a file
- [read_file](read_file.md), [read_text](read_text.md), [write_file](write_file.md),
  [append_file](append_file.md): a whole file in one call
- [buffered_reader](buffered_reader.md), [buffered_writer](buffered_writer.md): lines and blocks over a file
- [file_info](file_info.md), [permissions](permissions.md), [stat](stat.md): what [stat](file/stat.md) returns, the
  file system's side
- [map](map.md): a file mapped into memory
- [spawn_blocking](../async/spawn_blocking.md), [reactor](../async/readable.md): where the async forms wait
- `tests/io/rooted.cpp`: `rooted<io::file>` from `open` and from a handle (a copy of it, the same state), from an
  `expected` holding an error (the exception), `rooted<io::buffered_reader>` made in place, `rooted<io::buffer>` in
  a `std::vector` and in a global through collections.
- `tests/io/file.cpp`: create/write/read, the flags, `seek`/`read_at`/`truncate`/`stat`, a buffered file,
  `temp_file`, a pipe read by polling, the pool and the reactor from a task, a writer blocked by a full pipe, a file
  closed through an `io::reader` and through a `buffered_reader`, the handle (copies, a stream of a temporary, a
  `root_ptr` to it).
