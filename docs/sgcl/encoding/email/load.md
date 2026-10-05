[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::load, async_load

```cpp
static expected<email, error> load(const string& path);                                   // (1)
static expected<email, error> load(const string& path, const limits& l);                  // (2)
static async::task<expected<email, error>> async_load(string path) noexcept;              // (3)
static async::task<expected<email, error>> async_load(string path, limits l) noexcept;    // (4)
```

A message read from the file at `path` (an .eml) as [parse](parse.md) reads one.

- (1, 3) Within the default limits; (2, 4) within `l`.
- (3–4) The same for a task, the file read on the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `l` | the limits |

## Return value

The message, or the [error](../error/README.md) of [parse](parse.md), or `errc::io` with the file's `io_error()` for a file that cannot be read.

## Complexity

Linear in the size of the file.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    io::write_file("hello.eml", "Subject: Hello\r\n\r\nHi.\r\n");
    println("{}", encoding::email::load("hello.eml")->subject());
    println("{}", encoding::email::load("none.eml").error().message());
}
```

Output:

```text
Hello
input/output error: open none.eml: No such file or directory
```

## See also

- [parse](parse.md)
- [save, async_save](save.md)
- [email](README.md)
