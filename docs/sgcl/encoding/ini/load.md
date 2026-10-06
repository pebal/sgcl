[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::load, async_load

```cpp
static expected<ini, error> load(const string& path);                         // (1)
static async::task<expected<ini, error>> async_load(string path) noexcept;    // (2)
```

The sections of a file, read as [parse](parse.md) reads a stream with the default options: the one line a
program's configuration takes, `ini::load("app.ini")`.

1. Read now.
2. (1) for a task, the file read on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The sections, or the [error](../error/README.md): [parse](parse.md)'s, with its line and column, for the text; `io`
without a place, `io_error()` saying why, for a file that does not open or read.

## Complexity

Linear in the length of the file.

## Exceptions

- (1) What a read of the file throws.
- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("app.ini", "[server]\nhost = example.com\nport = 8080\n");
    auto config = encoding::ini::load("app.ini");
    println("{}:{}", config->get("server", "host", "?"), config->get_int("server", "port", 80));
    println(encoding::ini::load("missing.ini").error().message());
}
```

Output:

```text
example.com:8080
input/output error: open missing.ini: No such file or directory
```

## See also

- [save](save.md)
- [sgcl::encoding::ini](README.md)
