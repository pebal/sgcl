[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map/README.md) › [node_type](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type::operator=

```cpp
node_type& operator=(node_type&& other) noexcept;
```

Takes the node of `other` over; `other` is empty after. The element of the node this handle owned before, if
any, is destroyed first. Self-assignment does nothing. A handle is not copy-assignable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose node to take |

## Return value

`*this`.

## Complexity

Constant, plus the destructor of the element this handle owned.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Loud {
    int id;
    ~Loud() {
        println("~Loud {}", id);
    }
};

int main() {
    sorted_map<int, Loud> m;
    m.try_emplace(1, 1);
    m.try_emplace(2, 2);

    auto nh = m.extract(1);
    nh = m.extract(2);
    println("{} {}", nh.mapped().id, m.empty());
}
```

Output:

```text
~Loud 1
2 true
~Loud 2
```

## See also

- [(constructor)](sorted_map-node_type.md): constructs a handle
- [sgcl::sorted_map\<Key, T, Compare\>::node_type](README.md)
