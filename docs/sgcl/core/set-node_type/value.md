[sgcl](../../README.md) › [core](../README.md) › [set](../set/README.md) › [node_type](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::node_type::value

```cpp
value_type& value() const noexcept;
```

Returns the element of the node the handle holds. The reference is writable: out of a set the key may change,
and [insert](../set/insert.md) hashes it anew. The handle must not be [empty](empty.md). The handles of a set
have `value`; those of a map have `key` and `mapped` instead.

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
    set<string> s = {"draft", "final"};
    auto nh = s.extract("draft");
    nh.value() = "review";
    s.insert(std::move(nh));
    println("{} {} {}", s.size(), s.contains("review"), s.contains("draft"));
}
```

Output:

```text
2 true false
```

## See also

- [extract](../set/extract.md): takes a node out of a set
- [insert](../set/insert.md): links the node again
- [sgcl::set\<Key, Hash, KeyEqual\>::node_type](README.md)
