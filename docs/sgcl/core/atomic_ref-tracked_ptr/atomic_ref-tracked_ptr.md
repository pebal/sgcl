[sgcl](../../README.md) › [core](../README.md) › [atomic_ref](../atomic_ref.md) › [tracked_ptr](../atomic_ref-tracked_ptr.md)

# sgcl::atomic_ref\<tracked_ptr\<T\>\>::atomic_ref

```cpp
explicit atomic_ref(value_type& p) noexcept;    // (1)
atomic_ref(const atomic_ref& a) noexcept;       // (2)
```

1. A view of `p`: the operations act on its word. A `root_ptr` converts to the `tracked_ptr` it holds its object
   by, so `atomic_ref(root)` is a view of the root's word.
2. A copy of `a`: a view of the same pointer.

`p` must outlive the view, and must not be moved while a view of it exists.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer viewed |
| `a` | the view copied |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
    tracked_ptr<Node> next;
};

int main() {
    tracked_ptr node = make_tracked<Node>(1);
    atomic_ref next(node->next);  // atomic_ref<tracked_ptr<Node>>, deduced
    atomic_ref same = next;  // the same word
    next.store(make_tracked<Node>(2));
    println("{} {}", same.load()->value, node->next->value);

    static root_ptr<Node> global = make_tracked<Node>(3);  // a root lives anywhere
    atomic_ref root(global);  // the word the root holds its object by
    println("{}", root.load()->value);
}
```

Output:

```text
2 2
3
```

## See also

- [load, operator tracked_ptr\<T\>](load.md), [store](store.md): the operations on the word
- [sgcl::atomic_ref\<tracked_ptr\<T\>\>](../atomic_ref-tracked_ptr.md)
