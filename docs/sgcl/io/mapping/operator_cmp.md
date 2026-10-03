[sgcl](../../README.md) › [io](../README.md) › [mapping](README.md)

# sgcl::io::operator==, operator!= (sgcl::io::mapping)

```cpp
friend bool operator==(const mapping& a, const mapping& b) noexcept;
```

Checks whether two handles are the same mapping: whether they hold the same region, which the copies of one handle
share. Two mappings of one file made apart are two regions, and differ. Two empty handles are equal. `!=` is the
negation, which C++20 writes from `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when both hold the same region, or neither holds one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("data.txt", string("data"));
    io::mapping a = io::map("data.txt");
    io::mapping copy = a;
    io::mapping again = io::map("data.txt");
    println("{} {}", copy == a, again != a);
}
```

Output:

```text
true true
```

## See also

- [operator bool](operator_bool.md): whether a handle holds a mapping
- [sgcl::io::mapping](README.md)
