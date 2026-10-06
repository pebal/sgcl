[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::operator== (sgcl::encoding::yaml)

```cpp
friend bool operator==(const yaml& a, const yaml& b) noexcept;
```

Deep equality; `!=` is made from it by the compiler. Nodes of different kinds are never equal (`1` and `1.0`, `1`
and `"1"`); scalars by the value read (`0x10` is `16`, `1e1` is `10.0`, every `.nan` one value); strings by their
characters; sequences element by element; mappings as sets of members, in any order; the tags alike.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the nodes |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the nodes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto p = [](const char* t) { return encoding::yaml::parse(t).value(); };
    println("{} {} {} {}", p("0x10") == p("16"), p("1") == p("1.0"), p("{a: 1, b: 2}") == p("{b: 2, a: 1}"),
            p("x") == p("'x'"));
}
```

Output:

```text
true false true true
```

## See also

- [hash](hash.md)
- [sgcl::encoding::yaml](README.md)
