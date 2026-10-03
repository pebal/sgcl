[sgcl](../../README.md) › [io](../README.md) › [tee_reader](README.md)

# sgcl::io::tee_reader::tee_reader

```cpp
tee_reader(const io::reader& r, const io::writer& w) noexcept;
```

Constructs a reader of `r` that writes what it reads to `w`. Both are held as an [io::reader](../reader/README.md) and an
[io::writer](../writer/README.md): any stream converts to one, a handle by its object, a stream of the program's by
reference. Nothing is read until the first read.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the source |
| `w` | where what is read is written as well |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::tee_reader echo(io::buffer("seen twice\n"), io::stdout);
    auto text = echo.read_all_text();
    print("{}", *text);
}
```

Output:

```text
seen twice
seen twice
```

## See also

- [read, async_read](read.md)
- [sgcl::io::tee_reader](README.md)
