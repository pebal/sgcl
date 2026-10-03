[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set/README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::node_type

```cpp
#include "sgcl/core/ordered_set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash, class KeyEqual>
    class ordered_set {
    public:
        using node_type = /* a node handle */;
    };
}
```

`ordered_set<Key, Hash, KeyEqual>::node_type` is the node handle of an [ordered_set](../ordered_set/README.md), as
`std::unordered_set::node_type` is of its set: it owns one node that [extract](../ordered_set/extract.md) has
unlinked from a set, its element in it, untouched. Movable, not copyable. Out of a set the element may change,
which an iterator of the set never allows; [insert](../ordered_set/insert.md) links the node again, in the same set
or in another, at the end of its order, without copying or moving the element. A handle that dies holding a node
destroys the element; the node's memory is the collector's.

The type does not depend on `Hash` and `KeyEqual`: a node extracted from an `ordered_set` is inserted into one
with another hash and equality. It is not the node handle of [set](../set/README.md): the nodes of an `ordered_set` carry
the two links of the order, which a `set`'s have not.

## Rules

- The handle holds its node by a `tracked_ptr`, so it lives where one may: on a stack or inside a managed
  object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1).
- The element is destroyed by the handle that dies holding it, on a stack or inside a managed object destroyed by
  hand. A handle dying in a sweep, inside a managed object nobody refers to any more, leaves its node to the same
  sweep, which destroys the element.
- `value()` of an empty handle is undefined, as in `std`; nothing is checked.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `Key` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ordered_set-node_type.md) | constructs an empty handle, or takes the node of another |
| `(destructor)` | destroys the element of the node it holds; the node is left to the collector |
| [operator=](operator_assign.md) | takes the node of another handle |
| [empty](empty.md) | checks whether the handle holds no node |
| [operator bool](operator_bool.md) | checks whether the handle holds a node |
| [value](value.md) | the element, writable |
| [swap](swap.md) | exchanges the nodes of two handles |

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
    ordered_set<int> a = {1, 2};
    ordered_set<int, Mod10> b = {3};

    ordered_set<int>::node_type nh = a.extract(1);
    nh.value() = 11;
    b.insert(std::move(nh));  // into a set with another hash
    println("{} {} {}", a, b, nh.empty());

    using A = ordered_set<int>::node_type;
    println("{}", std::is_same_v<A, ordered_set<int, Mod10>::node_type>);
    println("{}", std::is_same_v<A, set<int>::node_type>);
}
```

Output:

```text
{2} {3, 11} true
true
false
```

## See also

- [extract](../ordered_set/extract.md): takes a node out of a set
- [insert](../ordered_set/insert.md): links the node of a handle
- [set](../set/README.md): the set without the order, whose node handle is another type
- [README: The rules](../README.md#the-rules)
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set/README.md)
