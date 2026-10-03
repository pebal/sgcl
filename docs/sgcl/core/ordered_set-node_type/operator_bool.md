[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set/README.md) › [node_type](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a node: `!empty()`. Explicit, so it converts in a condition (`if (nh)`) and
nowhere else.

## Parameters

None.

## Return value

`true` when the handle holds a node, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<string> s = {"a"};
    for (auto key : {"a", "b"}) {
        if (auto nh = s.extract(key)) {
            println("{} taken", nh.value());
        } else {
            println("{} absent", key);
        }
    }
}
```

Output:

```text
a taken
b absent
```

## See also

- [empty](empty.md): checks whether the handle holds no node
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type](README.md)
