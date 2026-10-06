[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::load, async_load

```cpp
static expected<dotenv, error> load(const string& path);                         // (1)
static async::task<expected<dotenv, error>> async_load(string path) noexcept;    // (2)
```

The entries of a file, read as [parse](parse.md) reads a stream with the default options: the one line a program
starts with, `dotenv::load(".env")`.

1. Read now.
2. (1) for a task, the file read on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The entries, or the [error](../error/README.md): [parse](parse.md)'s, with its line and column, for the text; `io`
without a place, `io_error()` saying why, for a file that does not open or read.

## Complexity

Linear in the length of the file.

## Exceptions

- (1) What a read of the file throws.\n- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file(".env", "HOST=example.com\nPORT=8080\n");
    auto env = encoding::dotenv::load(".env");
    println("{}:{}", env->get("HOST", "?"), env->get_int("PORT", 80));
    println(encoding::dotenv::load("missing.env").error().message());
}
```

Output:

```text
example.com:8080
input/output error: open missing.env: No such file or directory
```

## See also

- [save](save.md)
- [apply](apply.md)
- [sgcl::encoding::dotenv](README.md)
