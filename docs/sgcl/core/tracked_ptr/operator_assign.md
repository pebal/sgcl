[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](README.md)

# sgcl::tracked_ptr\<T\>::operator=

```cpp
tracked_ptr& operator=(std::nullptr_t) noexcept;                                                                              // (1)
tracked_ptr& operator=(const tracked_ptr& p) noexcept;                                                                        // (2)
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept;                                                                     // (3)
template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr& operator=(unique_ptr<U>&& u) noexcept;                                                                           // (4)
tracked_ptr& operator=(tracked_ptr&& p) noexcept;                                                                             // (5)
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
tracked_ptr& operator=(tracked_ptr<U>&& p) noexcept;                                                                          // (6)
```

Replaces the pointer: one store with the barrier, no temporary.

1. Drops the reference; the object lives on if anything else reaches it.
2. A copy of `p`.
3. A copy of `p` converted to a base class, or to `void`.
4. The object of `u`, released to the collector; `u` is empty after.
5. A copy of `p`: a move assignment is a copy, the source keeps its value.
6. The same as (3); the source keeps its value.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer to copy |
| `u` | the owner the object is taken from |

## Return value

`*this`.

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
    tracked_ptr head = make_tracked<Node>(1);
    head->next = make_tracked<Node>(2);  // from a unique_ptr
    tracked_ptr<Node> second;
    second = head->next;
    head = nullptr;  // the first Node is garbage, the second lives through second
    println("{} {}", second->value, head == nullptr);
}
```

Output:

```text
2 true
```

## See also

- [(constructor)](tracked_ptr.md): constructs the pointer
- [reset](reset.md): replaces the pointer with null or a raw one
- [sgcl::tracked_ptr\<T\>](README.md)
