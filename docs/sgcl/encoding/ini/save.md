[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::save, async_save

```cpp
expected<void, error> save(const string& path) const;                         // (1)
async::task<expected<void, error>> async_save(string path) const noexcept;    // (2)
```

The sections' [to_string](to_string.md) into a file, made or written over.

1. Written now.
2. (1) for a task: the sections copied into it, the file written on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

Nothing, or the [error](../error/README.md) `io`, without a place, for a file that does not open or write.

## Complexity

Linear in the size of the sections.

## Exceptions

- (1) What [to_string](to_string.md) throws, and what a write of the file throws.
- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::ini config = encoding::ini().set("server", "host", "example.com").set("server", "port", "8080");
    config.save("app.ini");
    print("{}", io::read_text("app.ini").value_or(string("?")));
    println(config.save("no/such/dir/app.ini").error().message());
}
```

Output:

```text
[server]
host = example.com
port = 8080
input/output error: open no/such/dir/app.ini: No such file or directory
```

## See also

- [load](load.md)
- [sgcl::encoding::ini](README.md)
