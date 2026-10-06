[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::operator==, operator!= (sgcl::io::library)

```cpp
friend bool operator==(const library& a, const library& b) noexcept;
```

Checks whether `a` and `b` are handles of the same library: copies of one handle. Two loads of one file are two
handles, though the loader shares the image. `a != b` is `!(a == b)`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same library, or both none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::library a = io::open_library(io::library_file_name("z")).value();
    io::library b = a;
    io::library c = io::open_library(io::library_file_name("z")).value();
    println("{} {}", a == b, a == c);
}
```

Output:

```text
true false
```

## See also

- [(constructor)](library.md)
- [sgcl::io::library](README.md)
