[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set/README.md)

# sgcl::sorted_set\<Key, Compare\>::node_type

```cpp
#include "sgcl/core/sorted_set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Compare = std::less<Key>>
    class sorted_set {
    public:
        using node_type = /* a node handle */;
    };
}
```

`sgcl::sorted_set<Key, Compare>::node_type` is the node handle of the set, what `std::set::node_type` is: the owner
of one node taken out of a tree by [extract](../sorted_set/extract.md), with its element in it. Through the handle the
element is reached and changed, the key too, since the node is in no tree; [insert](../sorted_set/insert.md) links the
node into a set again, this one or another, with no copy and no move of the element. A handle that dies with its
node destroys the element.

The type depends on `Key` alone: it is the `node_type` of every `sorted_set<Key, C>` and every
`sorted_multiset<Key, C>`, whatever the comparison, so a node passes between them.

## Rules

- A handle holds its node by a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object,
  never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1).
- A handle moves and does not copy: one node, one owner. A handle moved from is empty.
- The element is destroyed by the handle's destructor, or by an assignment over the handle, unless the node went
  back into a set first. A handle dying in a sweep, inside a managed object nobody refers to any more, leaves the
  element to the same sweep, which destroys it with the node.
- The node itself, its memory, is the collector's in every case.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `Key` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_set-node_type.md) | constructs an empty handle, or takes the node of another |
| `(destructor)` | destroys the element of a node the handle still owns |
| [operator=](operator_assign.md) | takes the node of another handle |
| [empty](empty.md) | checks whether the handle owns no node |
| [operator bool](operator_bool.md) | checks whether the handle owns a node |
| [value](value.md) | the element in the node |
| [swap](swap.md) | swaps the nodes of two handles |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>
#include <type_traits>

using namespace sgcl;

int main() {
    using handle = sorted_set<string>::node_type;
    println("{}", std::is_same_v<handle, sorted_multiset<string, std::greater<string>>::node_type>);

    sorted_set<string> drafts = {"intro", "outro"};
    sorted_multiset<string, std::greater<string>> archive;

    handle nh = drafts.extract("intro");
    nh.value() = "preface";
    archive.insert(std::move(nh));
    println("{} {} {}", drafts, archive, nh.empty());
}
```

Output:

```text
true
{"outro"} {"preface"} true
```

## See also

- [extract](../sorted_set/extract.md), [insert](../sorted_set/insert.md): take a node out, put it in
- [sorted_multiset](../sorted_multiset/README.md): the multiset with the same node handle
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set/README.md)
