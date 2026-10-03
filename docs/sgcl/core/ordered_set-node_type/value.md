[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md) › [node_type](../ordered_set-node_type.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type::value

```cpp
value_type& value() const noexcept;
```

Returns a reference to the element. It is writable: the node is out of any set, so an element changed here is
hashed afresh when the node is inserted. This is the way to change an element of a set, whose iterators only
read, without copying it. The handle must not be empty; nothing is checked.

## Parameters

None.

## Return value

A reference to the element.

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
    ordered_set<string> s = {"old", "other"};
    auto nh = s.extract("old");
    nh.value() = "new";
    s.insert(std::move(nh));
    println("{}", s);
}
```

Output:

```text
{"other", "new"}
```

## See also

- [extract](../ordered_set/extract.md): takes a node out of a set
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type](../ordered_set-node_type.md)
