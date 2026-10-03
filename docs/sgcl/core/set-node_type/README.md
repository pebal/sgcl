[sgcl](../../README.md) › [core](../README.md) › [set](../set/README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::node_type

```cpp
#include "sgcl/core/set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash, class KeyEqual>
    class set {
    public:
        using node_type = /* a node handle */;
    };
}
```

`set<Key, Hash, KeyEqual>::node_type` is the node handle of `std::unordered_set`: it owns one node taken out of a
set by [extract](../set/extract.md), the element in it untouched, and hands it to [insert](../set/insert.md) of a set,
which links the node again without copying or moving the element. Out of a set, the element is writable through
[value()](value.md): this is the way to change a key. A handle that dies without having been
inserted destroys its element; the node itself is reclaimed by the collector, as every node is.

The type depends on `Key` alone: it is the node handle of every set and every [multiset](../multiset/README.md) of that
`Key`, whatever their hashes and equalities, so a node moves between them. The handle of an
[ordered_set](../ordered_set/README.md) is another type, its nodes carrying the order of insertion.

## Rules

- The handle holds its node by a `tracked_ptr`, so it lives on a stack or inside a managed object: never in
  `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1).
- It is movable and not copyable: one handle owns a node.
- A handle dying in a sweep, inside a managed object nobody refers to any more, leaves its element to the same
  sweep, which destroys it with the node.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `Key` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](set-node_type.md) | constructs an empty handle, or takes the node of another |
| `(destructor)` | destroys the element of the node it holds, if any; the node is left to the collector |
| [operator=](operator_assign.md) | takes the node of another handle |
| [empty](empty.md) | checks whether the handle holds no node |
| [operator bool](operator_bool.md) | checks whether the handle holds a node |
| [value](value.md) | the element of the node |
| [swap](swap.md) | swaps the nodes of two handles |

## Non-member functions

| Function | Description |
|---|---|
| [swap](swap.md) | swaps the nodes of two handles |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    set<string> s = {"apple", "pear"};
    set<string>::node_type nh = s.extract("pear");
    nh.value() = "plum";

    multiset<string> m;
    m.insert(std::move(nh));  // no string copied
    println("{} {} {}", s.size(), m.contains("plum"), nh.empty());
    println("{}", std::is_same_v<set<string>::node_type, multiset<string>::node_type>);
}
```

Output:

```text
1 true true
true
```

## See also

- [extract](../set/extract.md), [insert](../set/insert.md): take a node out of a set, link it in
- [multiset](../multiset/README.md): shares the handle
- [sgcl::set\<Key, Hash, KeyEqual\>](../set/README.md)
