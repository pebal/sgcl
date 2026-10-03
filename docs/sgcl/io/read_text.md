[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_text, async_read_text

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> read_text(const string& path);                                // (1)
    async::task<expected<string, error>> async_read_text(const string& path) noexcept;    // (2)
}
```

The whole file at `path` as a [string](../core/string/README.md), in one call: opened, read, closed. The size comes from
`fstat` and the text is read straight into the string's object of that size, with no vector between; a file that
shrank since gives what it has, and one that grew is read on to its end. A file whose size is 0 (a FIFO, a file of
`/proc`) is read to the end its writer makes. Everything is read through the file opened, never by its path again,
which may name another file by then. The bytes are taken as they are:
nothing checks that they are UTF-8.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md): a disk has no readiness to wait for.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |

## Return value

The text of the file, empty for an empty file, or the [error](error/README.md) of the step that failed: of
[open](open.md) (`is_not_found()`, `is_permission()`), of `stat` or of `read` (`EISDIR` for a directory).

## Complexity

Linear in the size of the file.

## Exceptions

- (1) `length_error` when the file is longer than a string holds (4 GiB). `std::system_error` when the path names
  a FIFO or a device, which the file reads on the [reactor](../async/readable.md), the read has to wait, and the
  thread of the reactor, which its first use starts, cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("app.toml", "port = 8080");
    string config = io::read_text("app.toml").value_or("");
    println("{}", config);

    string fallback = io::read_text("missing.toml").value_or("port = 80");
    println("{}", fallback);
}
```

Output:

```text
port = 8080
port = 80
```

A task reads a file without holding a worker:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<size_t> count_lines(string path) {
    auto text = co_await io::async_read_text(path);
    if (!text) co_return 0;
    size_t lines = 0;
    for (char c : *text) {
        lines += c == '\n';
    }
    co_return lines;
}

int main() {
    io::write_file("poem.txt", "one\ntwo\nthree\n");
    println("{}", async::run(count_lines("poem.txt")));
}
```

Output:

```text
3
```

## See also

- [read_file](read_file.md): the whole file as bytes
- [read_lines](read_lines.md): the file's lines
- [write_file](write_file.md): the other direction
- [read_all_text](mixin/reader/read_all_text.md): the rest of an open file as a `string`
- [sgcl::io::file](file/README.md)
