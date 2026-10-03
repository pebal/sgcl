[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::root_ptr

```cpp
root_ptr() noexcept;                                                                                                          // (1)
root_ptr(std::nullptr_t) noexcept;                                                                                            // (2)
template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
root_ptr(const tracked_ptr<U>& p) noexcept;                                                                                   // (3)
template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
root_ptr(unique_ptr<U>&& u) noexcept;                                                                                         // (4)
root_ptr(const root_ptr& o) noexcept;                                                                                         // (5)
template<class U, std::enable_if_t<std::is_convertible_v<typename root_ptr<U>::element_type*, element_type*>, int> = 0>
root_ptr(const root_ptr<U>& o) noexcept;                                                                                      // (6)
root_ptr(root_ptr&& o) noexcept;                                                                                              // (7)
```

Takes a cell from the thread's cell allocator and stores in it the pointer from one of the sources below. Every
constructor takes a cell, the null ones included: the `root_ptr` may be assigned to later.

1. A null pointer.
2. The same as (1).
3. The pointer of `p`, `U*` convertible to `T*`.
4. The object of `u`, released from its owner as a `tracked_ptr` takes it: `root_ptr<T> r = make_tracked<T>(...)`.
   `u` is empty after.
5. The pointer of `o`, in a cell of its own.
6. The pointer of `o`, `U*` convertible to `T*`, in a cell of its own.
7. The pointer of `o`, in a cell of its own; `o` is null after and keeps its cell.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer to store |
| `u` | the owner the object is taken from |
| `o` | the root whose pointer is stored |

## Complexity

Constant: a cell from the thread's allocator, and one managed allocation per block of cells.

## Exceptions

None.

## Notes

The constructor registers the thread with the collector, as a `tracked_ptr`'s does. No move takes a cell from
another `root_ptr`: a thread reading through the cell of a `root_ptr` another thread moves from reads a cell that
lives as long as its `root_ptr`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

struct Node {
    int value;
    tracked_ptr<Node> next;
};

int main() {
    std::vector<root_ptr<Node>> handles;  // roots on the unmanaged heap
    handles.emplace_back(make_tracked<Node>(1));
    tracked_ptr<Node> first = handles[0];
    root_ptr from_tracked = first;
    root_ptr copy = handles[0];  // a cell of its own
    root_ptr<Node> none;
    println("{} {} {}", from_tracked->value, copy == first, none == nullptr);

    root_ptr moved = std::move(handles[0]);
    println("{} {}", handles[0] == nullptr, moved->value);
}
```

Output:

```text
1 true true
true 1
```

## See also

- [operator=](operator_assign.md): stores another pointer in the cell
- [ptr, operator tracked_ptr\<T\>&](ptr.md): the cell's word
- [sgcl::root_ptr\<T\>](../root_ptr.md)
