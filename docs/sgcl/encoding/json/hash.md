[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::hash

```cpp
size_t hash() const noexcept;
```

The hash of the value, alike for values that are [equal](operator_cmp.md): a number by its double (`1` and `1.0`
alike), an array by its elements in order, an object by its members in any order. A container's hash is computed
walking the tree with a stack of its own, so a value deeper than any stack of calls is hashed as well.

## Parameters

None.

## Return value

The hash.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json a = encoding::json::parse(R"({"x": 1, "y": [2, 3]})");
    encoding::json b = encoding::json::parse(R"({"y": [2.0, 3], "x": 1.0})");
    encoding::json c = encoding::json::parse(R"({"x": 1, "y": [3, 2]})");
    println("{} {}", a == b, a.hash() == b.hash());
    println("{} {}", a == c, a.hash() == c.hash());
}
```

Output:

```text
true true
false false
```

## See also

- [operator==](operator_cmp.md): equality by value
- [sgcl::encoding::json](README.md)
