[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set/README.md) › [node_type](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type::operator=

```cpp
node_type& operator=(node_type&& other) noexcept;
```

Destroys the element of the node this handle holds, if any, and takes the node of `other` over; `other` is empty
after. Assigning a handle to itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle the node is taken from |

## Return value

`*this`.

## Complexity

Constant, plus the destructor of the element this handle held.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<string> s = {"one", "two"};
    auto nh = s.extract("one");
    nh = s.extract("two");  // "one" is destroyed here
    println("{} {}", nh.value(), s.empty());
}
```

Output:

```text
two true
```

## See also

- [(constructor)](ordered_set-node_type.md): constructs a handle
- [swap](swap.md): exchanges the nodes of two handles
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type](README.md)
