[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](README.md)

# sgcl::tracked_ptr\<T\>::to_shared

```cpp
std::shared_ptr<element_type> to_shared() const noexcept;
```

The object held from unmanaged memory. Returns a `std::shared_ptr` to the object whose control block owns a managed
holder of this pointer: the holder is a root (its `unique_ptr` lives in the control block), so the object is
reachable for as long as any copy of the `shared_ptr` lives, and the `shared_ptr` itself may live anywhere a
`tracked_ptr` may not: `new` memory, a `std` container, a global, a lambda copied to the heap or run on another
thread. Copies of the `shared_ptr` share the control block and the holder.

The object stays managed: when the last `shared_ptr` is gone the holder is released and the object lives on if
anything else reaches it, and is destroyed on a collector thread, under the rules of destructors, once nothing
does.

## Parameters

None.

## Return value

A `shared_ptr` to the object; a null `shared_ptr` for a null pointer. An alias into a member gives a `shared_ptr`
to the member that keeps the whole object.

## Complexity

Constant: two allocations, the holder on the managed heap and the control block on the unmanaged one.

## Exceptions

None.

## Notes

Only the call allocates; a copy of the `shared_ptr` does not. The two allocations are why this is a named function
and not a conversion: the cost and the new root are visible where they are paid. A `shared_ptr` from `to_shared()`
is not a deterministic owner the way `unique_ptr` is.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>
#include <vector>

using namespace sgcl;

struct Node {
    int value = 7;
    tracked_ptr<Node> next;
};

int main() {
    std::vector<std::shared_ptr<Node>> kept;  // a std container: no tracked_ptr may live in it
    tracked_ptr node = make_tracked<Node>();
    node->next = make_tracked<Node>();
    kept.push_back(node.to_shared());         // the Node and its next live while the shared_ptr does
    node = nullptr;

    collector::force_collect(true);  // optional, for the demonstration
    println("{} {}", kept[0]->value, kept[0]->next->value);
    println("{}", tracked_ptr<Node>().to_shared() == nullptr);
    kept.clear();  // the last copy: the holder is released, the Node is garbage
}
```

Output:

```text
7 7
true
```

## See also

- [root_ptr](../root_ptr/README.md): a root of its own, anywhere
- [unique_ptr](../unique_ptr/README.md): one deterministic owner
- [sgcl::tracked_ptr\<T\>](README.md)
