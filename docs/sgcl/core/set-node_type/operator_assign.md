[sgcl](../../README.md) › [core](../README.md) › [set](../set.md) › [node_type](../set-node_type.md)

# sgcl::set\<Key, Hash, KeyEqual\>::node_type::operator=

```cpp
node_type& operator=(node_type&& other) noexcept;
```

Takes the node of `other` over: the element of the node this handle held, if any, is destroyed first, and
`other` is left empty. An assignment of a handle to itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose node to take |

## Return value

`*this`.

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
    auto nh = s.extract("a");
    nh = s.extract("b");  // "a" is destroyed here
    println("{} {}", nh.value(), s.empty());

    set<string>::node_type other;
    other = std::move(nh);
    println("{} {}", nh.empty(), other.value());
}
```

Output:

```text
b true
true b
```

## See also

- [(constructor)](set-node_type.md): constructs a handle
- [swap](swap.md): exchanges the nodes of two handles
- [sgcl::set\<Key, Hash, KeyEqual\>::node_type](../set-node_type.md)
