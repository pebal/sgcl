[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::operator=

```cpp
/*(1)*/ root_ptr& operator=(const root_ptr& o) noexcept;
/*(2)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename root_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(const root_ptr<U>& o) noexcept;
/*(3)*/ root_ptr& operator=(root_ptr&& o) noexcept;
/*(4)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(const tracked_ptr<U>& p) noexcept;
/*(5)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        root_ptr& operator=(unique_ptr<U>&& u) noexcept;
/*(6)*/ root_ptr& operator=(std::nullptr_t) noexcept;
```

Stores another pointer in this root's cell: a store into the cell's `tracked_ptr`, with its barrier. The cell stays;
no assignment allocates.

1. The pointer of `o`.
2. The pointer of `o`, `U*` convertible to `T*`.
3. The pointer of `o`; `o` is null after and keeps its cell. A move into itself changes nothing.
4. The pointer of `p`.
5. The object of `u`, released to the collector; `u` is empty after.
6. Null: the object lives on if anything else reaches it.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the root whose pointer is stored |
| `p` | the pointer to store |
| `u` | the owner the object is taken from |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Notes

No store allocates, so two threads storing into the same `root_ptr` race on one atomic word, as they do on a
`tracked_ptr`, and never on the making of a cell.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    root_ptr<int> current;
    current = make_tracked<int>(1);
    root_ptr<int> previous;
    previous = current;
    current = tracked_ptr(make_tracked<int>(2));
    println("{} {}", *previous, *current);

    previous = std::move(current);
    println("{} {}", *previous, current == nullptr);
}
```

Output:

```text
1 2
2 true
```

## See also

- [(constructor)](root_ptr.md): takes a cell and stores the pointer in it
- [reset](reset.md): stores null or another pointer
- [sgcl::root_ptr\<T\>](../root_ptr.md)
