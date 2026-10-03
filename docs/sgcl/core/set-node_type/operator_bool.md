[sgcl](../../README.md) › [core](../README.md) › [set](../set.md) › [node_type](../set-node_type.md)

# sgcl::set\<Key, Hash, KeyEqual\>::node_type::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a node: `!empty()`, for a test such as `if (auto nh = s.extract(key))`.

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
    set<string> s = {"a", "b"};
    for (const char* key : {"a", "z"}) {
        if (auto nh = s.extract(key)) {
            println("{} extracted", nh.value());
        } else {
            println("{} not there", key);
        }
    }
}
```

Output:

```text
a extracted
z not there
```

## See also

- [empty](empty.md): checks whether the handle holds no node
- [sgcl::set\<Key, Hash, KeyEqual\>::node_type](../set-node_type.md)
