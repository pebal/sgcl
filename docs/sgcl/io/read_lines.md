[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_lines, async_read_lines

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<vector<string>, error> read_lines(const string& path);                                // (1)
    async::task<expected<vector<string>, error>> async_read_lines(const string& path) noexcept;    // (2)
}
```

The lines of the text file at `path`, in one call: opened, read, closed. The lines are those of
[buffered_reader::read_line](buffered_reader/read_line.md): each without its `"\n"` and without a `"\r"` before it,
so a file written on Windows reads as one written on POSIX; the last line of a file that does not end in `"\n"` is a
line too, and a file that ends in `"\n"` has no empty line after it. An empty file has no lines. The file is read a
block at a time and never held whole: what stays in memory is the lines. The bytes are taken as they are: nothing
checks that they are UTF-8.

The lines come as a `vector`, as [read_text](read_text.md) gives a `string`: the error, if there is one, is known
before the first line is used. A file too large to keep its lines, or a stream read as it comes, is read by
[buffered_reader::lines](buffered_reader/lines.md) instead, one line at a time.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md): a disk has no readiness to wait for.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |

## Return value

The lines of the file, none for an empty file, or the [error](error/README.md) of the step that failed, the lines read
before it dropped: of [open](open.md) (`is_not_found()`, `is_permission()`) or of a read (`EISDIR` for a directory).

## Complexity

Linear in the size of the file.

## Exceptions

- (1) `length_error` when a line is longer than a string holds (4 GiB). `std::system_error` when the path names a
  FIFO or a device, which the file reads on the [reactor](../async/readable.md), the read has to wait, and the thread
  of the reactor, which its first use starts, cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("hosts.txt", "alpha\r\nbeta\n\ngamma");
    auto hosts = io::read_lines("hosts.txt");
    for (auto& host : *hosts) {
        println("[{}]", host);
    }

    auto missing = io::read_lines("missing.txt");
    println("{} {}", missing.has_value(), missing.error().is_not_found());
}
```

Output:

```text
[alpha]
[beta]
[]
[gamma]
false true
```

A task reads the lines without holding a worker:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<size_t> count_todos(string path) {
    auto lines = co_await io::async_read_lines(path);
    if (!lines) co_return 0;
    size_t todos = 0;
    for (auto& line : *lines) {
        todos += line.starts_with("TODO");
    }
    co_return todos;
}

int main() {
    io::write_file("notes.txt", "TODO tests\ndone docs\nTODO bench\n");
    println("{}", async::run(count_todos("notes.txt")));
}
```

Output:

```text
2
```

## See also

- [read_text](read_text.md): the whole file as one string
- [buffered_reader::lines](buffered_reader/lines.md): the lines of a stream, one at a time
- [write_file](write_file.md): the other direction
- [sgcl::io::file](file/README.md)
