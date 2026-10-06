[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::type

```cpp
kind type() const noexcept;
```

The [kind](../toml-kind.md) of the value.

## Parameters

None.

## Return value

The kind.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("a = ['x', 1, 1.5, true, 1979-05-27T07:32:00Z, 1979-05-27T07:32:00, 1979-05-27, 07:32:00, [], {}]").value();
    for (const auto& e : v["a"].elements()) {
        println("{} {}", e.text(), int(e.type()));
    }
}
```

Output:

```text
x 1
1 2
1.5 3
true 4
1979-05-27T07:32:00Z 5
1979-05-27T07:32:00 6
1979-05-27 7
07:32:00 8
 9
 0
```

## See also

- [toml::kind](../toml-kind.md)
- [sgcl::encoding::toml](README.md)
