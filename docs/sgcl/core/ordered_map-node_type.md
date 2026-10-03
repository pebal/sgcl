[sgcl](../README.md) › [core](README.md) › [ordered_map](ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::node_type

```cpp
#include "sgcl/core/ordered_map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash, class KeyEqual>
    class ordered_map {
    public:
        using node_type = /* a node handle */;
    };
}
```

`ordered_map<Key, T, Hash, KeyEqual>::node_type` is the node handle of an [ordered_map](ordered_map.md), as
`std::unordered_map::node_type` is of its map: it owns one node that [extract](ordered_map/extract.md) has
unlinked from a map, its element in it, untouched. Movable, not copyable. Out of a map the key may change;
[insert](ordered_map/insert.md) links the node again, in the same map or in another, at the end of its order,
without copying or moving the element. A handle that dies holding a node destroys the element; the node's
memory is the collector's.

The type does not depend on `Hash` and `KeyEqual`: a node extracted from an `ordered_map` is inserted into one
with another hash and equality. It is not the node handle of [map](map.md): the nodes of an `ordered_map` carry
the two links of the order, which a `map`'s have not.

## Rules

- The handle holds its node by a `tracked_ptr`, so it lives where one may: on a stack or inside a managed
  object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](README.md#the-rules), 1).
- The element is destroyed by the handle that dies holding it, on a stack or inside a managed object destroyed by
  hand. A handle dying in a sweep, inside a managed object nobody refers to any more, leaves its node to the same
  sweep, which destroys the element.
- `key()` and `mapped()` of an empty handle are undefined, as in `std`; nothing is checked.

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ordered_map-node_type/ordered_map-node_type.md) | constructs an empty handle, or takes the node of another |
| `(destructor)` | destroys the element of the node it holds; the node is left to the collector |
| [operator=](ordered_map-node_type/operator_assign.md) | takes the node of another handle |
| [empty](ordered_map-node_type/empty.md) | checks whether the handle holds no node |
| [operator bool](ordered_map-node_type/operator_bool.md) | checks whether the handle holds a node |
| [key](ordered_map-node_type/key.md) | the key of the element, writable |
| [mapped](ordered_map-node_type/mapped.md) | the value of the element |
| [swap](ordered_map-node_type/swap.md) | exchanges the nodes of two handles |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Mod10 {
    size_t operator()(int key) const noexcept {
        return size_t(key % 10);
    }
};

int main() {
    ordered_map<int, string> a = {{1, "one"}, {2, "two"}};
    ordered_map<int, string, Mod10> b = {{3, "three"}};

    ordered_map<int, string>::node_type nh = a.extract(1);
    nh.key() = 11;
    b.insert(std::move(nh));  // into a map with another hash
    println("{} {} {}", a, b, nh.empty());

    using A = ordered_map<int, string>::node_type;
    println("{}", std::is_same_v<A, ordered_map<int, string, Mod10>::node_type>);
    println("{}", std::is_same_v<A, map<int, string>::node_type>);
}
```

Output:

```text
{2: "two"} {3: "three", 11: "one"} true
true
false
```

## See also

- [extract](ordered_map/extract.md): takes a node out of a map
- [insert](ordered_map/insert.md): links the node of a handle
- [map](map.md): the map without the order, whose node handle is another type
- [README: The rules](README.md#the-rules)
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](ordered_map.md)
