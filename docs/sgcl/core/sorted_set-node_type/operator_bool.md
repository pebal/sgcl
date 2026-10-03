[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set/README.md) › [node_type](README.md)

# sgcl::sorted_set\<Key, Compare\>::node_type::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle owns a node: `!empty()`. Explicit, so a handle is tested in a condition and does not
convert to `bool` elsewhere.

## Parameters

None.

## Return value

`true` when the handle owns a node, `false` otherwise.

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
    sorted_set<int> numbers = {4, 8};

    for (int key : {8, 15}) {
        if (auto nh = numbers.extract(key)) {
            println("took {}", nh.value());
        } else {
            println("no {}", key);
        }
    }
}
```

Output:

```text
took 8
no 15
```

## See also

- [empty](empty.md): checks whether the handle owns no node
- [sgcl::sorted_set\<Key, Compare\>::node_type](README.md)
