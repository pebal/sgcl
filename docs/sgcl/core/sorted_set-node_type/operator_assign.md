[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set/README.md) › [node_type](README.md)

# sgcl::sorted_set\<Key, Compare\>::node_type::operator=

```cpp
node_type& operator=(node_type&& other) noexcept;
```

Takes the node of `other`, which is left empty. The element of the node this handle owned before, if any, is
destroyed first. Assigning a handle to itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle to take the node from |

## Return value

`*this`.

## Complexity

Constant, and the destructor of the element this handle owned.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Noisy {
    int id;

    ~Noisy() {
        println("~{}", id);
    }

    bool operator<(const Noisy& other) const noexcept {
        return id < other.id;
    }
};

int main() {
    sorted_set<Noisy> set;
    set.emplace(1);
    set.emplace(2);

    auto nh = set.extract(set.begin());
    println("assign");
    nh = set.extract(set.begin());  // 1 is destroyed here
    println("{} {}", nh.value().id, set.size());
}
```

Output:

```text
assign
~1
2 0
~2
```

## See also

- [(constructor)](sorted_set-node_type.md): constructs a handle
- [sgcl::sorted_set\<Key, Compare\>::node_type](README.md)
