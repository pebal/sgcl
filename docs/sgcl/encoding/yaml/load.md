[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::load, async_load

```cpp
static expected<yaml, error> load(const string& path);                         // (1)
static async::task<expected<yaml, error>> async_load(string path) noexcept;    // (2)
```

The one document of a file, read as [parse](parse.md) reads a stream: the one line a program's configuration takes,
`yaml::load("app.yaml")`. A file of several documents is read by [parse_all](parse_all.md) of its text.

1. Read now.
2. (1) for a task, the file read on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The node, or the [error](../error/README.md): [parse](parse.md)'s, with its line and column, for the text; `io`
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
    io::write_file("app.yaml", "name: app\nserver:\n  port: 8080\n");
    auto config = encoding::yaml::load("app.yaml");
    println("{} {}", config->operator[]("name").as_string("?"), config->operator[]("server")["port"].as_int(80));
    println(encoding::yaml::load("missing.yaml").error().message());
}
```

Output:

```text
app 8080
input/output error: open missing.yaml: No such file or directory
```

## See also

- [save](save.md)
- [parse](parse.md)
- [sgcl::encoding::yaml](README.md)
