[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the file was closed by [close](close.md), through this handle or another of the same file, or
through a stream made of it.

## Parameters

None.

## Return value

`true` once `close` was called, `false` before.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file f = io::create("data.txt");
    io::writer out = f;
    println("{}", f.is_closed());
    out.close();
    println("{}", f.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md): ends the file
- [writer](../writer/README.md): a stream made of a file holds the same file
- [sgcl::io::file](README.md)
