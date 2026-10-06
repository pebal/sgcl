[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::type

```cpp
kind type() const noexcept;
```

The [kind](../yaml-kind.md) of the node: what the core schema
read a scalar as, or the collection.

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
    for (const auto& v : encoding::yaml::parse("[~, true, 0o17, 1.5, '1.5', [], {}]")->elements()) {
        println("{} {}", v.to_json().to_string(), int(v.type()));
    }
}
```

Output:

```text
null 0
true 1
15 2
1.5 3
"1.5" 4
[] 5
{} 6
```

## See also

- [yaml::kind](../yaml-kind.md)
- [sgcl::encoding::yaml](README.md)
