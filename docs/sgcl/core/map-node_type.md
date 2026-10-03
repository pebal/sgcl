[sgcl](../README.md) › [core](README.md) › [map](map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::node_type

```cpp
#include "sgcl/core/map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map {
    public:
        using node_type = /* a node handle */;
    };
}
```

`sgcl::map<Key, T, Hash, KeyEqual>::node_type` is the node handle of a [map](map.md), as
`std::unordered_map::node_type` is: it owns one node unlinked from a map, with its element, which
[extract](map/extract.md) hands out and [insert](map/insert.md) links into a map again, without copying or moving
the element. The handle is movable, not copyable. The element is destroyed when the handle dies without having
been inserted; the node's memory is the collector's.

The type depends on `Key` and `T` alone: it is the `node_type` of every map and [multimap](multimap.md) of the
same `Key` and `T`, whatever their hashes and equalities, so a node extracted from one goes into any other.

## Rules

- A handle holds its node by a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object,
  never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](README.md#the-rules), 1).
- The element is destroyed with the handle, wherever that runs, but for a handle dying in a sweep, inside a managed
  object nobody refers to any more: its node is garbage of the same sweep, and destroys its element when the
  sweep reaches it.
- Out of a map, the key may be changed through [key()](map-node_type/key.md); the map hashes it again when the
  node is inserted.

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](map-node_type/map-node_type.md) | constructs an empty handle, or takes another's node over |
| `(destructor)` | destroys the element of the node it holds, if any; the node is left to the collector |
| [operator=](map-node_type/operator_assign.md) | takes another handle's node over |
| [empty](map-node_type/empty.md) | checks whether the handle holds no node |
| [operator bool](map-node_type/operator_bool.md) | checks whether the handle holds a node |
| [key](map-node_type/key.md) | the key of the element, writable |
| [mapped](map-node_type/mapped.md) | the value of the element |
| [swap](map-node_type/swap.md) | swaps the nodes of two handles |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Mod10 {
    size_t operator()(int x) const noexcept { return size_t(x % 10); }
};

int main() {
    using handle = map<int, string>::node_type;
    println("{}", std::is_same_v<handle, multimap<int, string, Mod10>::node_type>);

    map<int, string> m = {{1, "one"}, {2, "two"}};
    multimap<int, string, Mod10> mm;
    handle nh = m.extract(1);
    nh.key() = 11;
    mm.insert(std::move(nh));
    println("{} {} {}", m.size(), mm.find(11)->second, nh.empty());
}
```

Output:

```text
true
1 one true
```

## See also

- [extract](map/extract.md), [insert](map/insert.md): take a node out of a map, link it into one
- [merge](map/merge.md): relinks the nodes of another map
- [sgcl::map\<Key, T, Hash, KeyEqual\>](map.md), [sgcl::multimap\<Key, T, Hash, KeyEqual\>](multimap.md)
