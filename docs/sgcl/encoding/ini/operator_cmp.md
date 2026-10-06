[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::operator== (sgcl::encoding::ini)

```cpp
friend bool operator==(const ini& a, const ini& b) noexcept;
```

Whether the sections have the same names and the same entries in the same order; `!=` is made from it by the
compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the sections |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the sections.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto p = [](const char* t) { return encoding::ini::parse(t).value(); };
    println("{} {}", p("[a]\nx=1") == p("[a]\n  x : 1"), p("[a]\nx=1\n[b]") == p("[b]\n[a]\nx=1"));
}
```

Output:

```text
true false
```

## See also

- [sections](sections.md)
- [sgcl::encoding::ini](README.md)
