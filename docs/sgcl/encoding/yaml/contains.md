[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::contains

```cpp
bool contains(const yaml& key) const noexcept;
```

Whether a mapping has the key, by deep equality: what [operator[]](operator_at.md) cannot tell from a key whose value
is null.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

`true` when the mapping has it.

## Complexity

Linear in the members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::yaml::parse("a: ~\n1: one").value();
    println("{} {} {} {}", v.contains("a"), v.contains("b"), v.contains(1), v["a"].is_null());
}
```

Output:

```text
true false true true
```

## See also

- [operator\\[\\]](operator_at.md)
- [sgcl::encoding::yaml](README.md)
