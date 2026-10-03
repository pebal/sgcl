[sgcl](../README.md) › [core](README.md) › [sorted_map](sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::node_type

```cpp
#include "sgcl/core/sorted_map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Compare>
    class sorted_map {
    public:
        using node_type = /* a node handle */;
    };
}
```

`sorted_map<Key, T, Compare>::node_type` is the node handle of a sorted map, as `std::map::node_type` is: it owns
one node taken out of a map by [extract](sorted_map/extract.md), with its element, and gives it to
[insert](sorted_map/insert.md) of a map with the same `Key` and `T`, which links the node without copying the
element. Out of a map the key may be changed, through [key](sorted_map-node_type/key.md). The handle is movable,
not copyable; when it dies without having been inserted it destroys the element, and the node's memory is the
collector's.

It is one type for every comparator and the same type as
[sorted_multimap](sorted_multimap.md)`<Key, T, Compare>::node_type`: a node moves between a map and a multimap
of the same `Key` and `T`.

## Rules

- A handle holds its node by a `tracked_ptr`, so it lives where the map may: on a stack or inside a managed
  object, never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](README.md#the-rules), 1).
- A handle dying in a sweep, inside a managed object nobody refers to any more, leaves its node to the same sweep,
  which destroys the element.
- `key()` and `mapped()` may be called only on a handle that is not empty.

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_map-node_type/sorted_map-node_type.md) | constructs an empty handle, or takes another's node |
| `(destructor)` | destroys the element of a node that was not inserted; the node is the collector's |
| [operator=](sorted_map-node_type/operator_assign.md) | takes another handle's node |
| [empty](sorted_map-node_type/empty.md) | checks whether the handle owns no node |
| [operator bool](sorted_map-node_type/operator_bool.md) | checks whether the handle owns a node |
| [key](sorted_map-node_type/key.md) | the key of the element, writable |
| [mapped](sorted_map-node_type/mapped.md) | the mapped value of the element |
| [swap](sorted_map-node_type/swap.md) | swaps the nodes of two handles |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    using handle = sorted_map<int, string>::node_type;
    println("{}", std::is_same_v<handle, sorted_multimap<int, string>::node_type>);

    sorted_map<int, string> m = {{1, "one"}, {2, "two"}};
    sorted_multimap<int, string> mm = {{2, "deux"}};

    handle nh = m.extract(2);
    println("{} {} {}", nh.empty(), nh.key(), nh.mapped());
    mm.insert(std::move(nh));  // into a multimap: no string is copied
    println("{} {} {}", m, mm, nh.empty());
}
```

Output:

```text
true
false 2 two
{1: "one"} {2: "deux", 2: "two"} true
```

## See also

- [extract](sorted_map/extract.md): takes a node out of a map
- [insert](sorted_map/insert.md): links a node into a map
- [sgcl::sorted_map\<Key, T, Compare\>](sorted_map.md), [sgcl::sorted_multimap\<Key, T, Compare\>](sorted_multimap.md)
