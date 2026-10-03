[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_file, async_read_file

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<vector<byte>, error> read_file(const string& path);                                // (1)
    async::task<expected<vector<byte>, error>> async_read_file(const string& path) noexcept;    // (2)
}
```

The whole file at `path` in one call: opened, read, closed. The size comes from `fstat`, one buffer of that size is
made and filled by one read, and more are made only should the file have grown since: a file that shrank gives what
it has. Go's `os.ReadFile`.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md): a disk has no readiness to wait for.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |

## Return value

The bytes of the file, or the [error](error.md) of the step that failed: of [open](open.md) (`is_not_found()`,
`is_permission()`), of `stat` or of `read` (`EISDIR` for a directory).

## Complexity

Linear in the size of the file.

## Exceptions

- (1) `std::system_error` when the path names a FIFO or a device, which the file reads on the
  [reactor](../async/readable.md), the read has to wait, and the thread of the reactor, which its first use starts,
  cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> bytes = {byte{1}, byte{2}, byte{3}};
    io::write_file("data.bin", bytes);
    auto data = io::read_file("data.bin");
    println("{} {}", data->size(), *data);

    auto missing = io::read_file("missing.bin");
    println("{}", missing.error().message());
}
```

Output:

```text
3 [1, 2, 3]
open missing.bin: No such file or directory
```

A task reads files without holding a worker:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<size_t> total(vector<string> paths) {
    size_t sum = 0;
    for (const string& path : paths) {
        auto data = co_await io::async_read_file(path);
        sum += data ? data->size() : 0;
    }
    co_return sum;
}

int main() {
    io::write_file("a.txt", "four");
    io::write_file("b.txt", "seven..");
    println("{}", async::run(total({"a.txt", "b.txt", "missing.txt"})));
}
```

Output:

```text
11
```

## See also

- [read_text](read_text.md): the whole file as a `string`
- [write_file](write_file.md): the other direction
- [read_all](mixin/reader/read_all.md): the rest of an open file
- [map](map.md): a file mapped into memory instead of read
- [sgcl::io::file](file.md)
