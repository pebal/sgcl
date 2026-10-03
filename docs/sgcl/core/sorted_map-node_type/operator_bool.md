[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md) › [node_type](../sorted_map-node_type.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle owns a node: `!empty()`, for a test in an `if`.

## Parameters

None.

## Return value

`true` when the handle owns a node.

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
    sorted_map<string, int> m = {{"a", 1}};
    for (const char* key : {"a", "b"}) {
        if (auto nh = m.extract(key)) {
            println("{} taken out", nh.key());
        } else {
            println("no {}", key);
        }
    }
}
```

Output:

```text
a taken out
no b
```

## See also

- [empty](empty.md): checks whether the handle owns no node
- [sgcl::sorted_map\<Key, T, Compare\>::node_type](../sorted_map-node_type.md)
