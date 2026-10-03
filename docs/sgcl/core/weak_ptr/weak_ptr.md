[sgcl](../../README.md) › [core](../README.md) › [weak_ptr](../weak_ptr.md)

# sgcl::weak_ptr\<T\>::weak_ptr

```cpp
/*(1)*/ constexpr weak_ptr() noexcept = default;
/*(2)*/ constexpr weak_ptr(std::nullptr_t) noexcept;
/*(3)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        weak_ptr(const tracked_ptr<U>& p) noexcept;
/*(4)*/ template<class U, std::enable_if_t<std::is_convertible_v<U*, element_type*>, int> = 0>
        weak_ptr(const root_ptr<U>& r) noexcept;
/*(5)*/ weak_ptr(const weak_ptr&) noexcept = default;
/*(6)*/ weak_ptr(weak_ptr&&) noexcept = default;
/*(7)*/ template<class U, std::enable_if_t<std::is_same_v<std::remove_cv_t<U>, std::remove_cv_t<T>> && std::is_convertible_v<U*, T*>, int> = 0>
        weak_ptr(const weak_ptr<U>& w) noexcept;
```

Constructs a weak pointer from one of the sources below.

1. An empty `weak_ptr`, with no cell: expired.
2. The same as (1).
3. A cell of its own holding the object of `p`, `U*` convertible to `T*`; an empty `weak_ptr` when `p` is null.
4. The same as (3) for the pointer of a root: `weak_ptr w = root;`.
5. Shares the cell of the other `weak_ptr`.
6. The same as (5): a move is a copy of the word, the source keeps its cell.
7. Shares the cell of `w`, a `weak_ptr` to `T` without `const`: the same type with `const` added. A `weak_ptr` to a
   derived class does not convert to a `weak_ptr` of its base or to `weak_ptr<void>` (see Notes).

## Parameters

| Parameter | Description |
|---|---|
| `p` | the pointer to the object to observe |
| `r` | the root whose object is observed |
| `w` | the `weak_ptr` whose cell is shared |

## Complexity

Constant: (3–4) one managed allocation, the cell of 16 bytes.

## Exceptions

None.

## Notes

The object of `p` may not be one a `unique_ptr` owns: debug builds assert it. Hand the object to the collector
first.

Unlike `std::weak_ptr`, (7) takes no `weak_ptr` to a derived class: the cell holds the address `lock()` returns,
and the address of a base may lie at an offset that only the derived type or the live object gives (a virtual base
is found through the object). Such a conversion would lock the object to convert it, and a second `lock()` would
follow to use it. A `weak_ptr` of a base is made from a strong pointer, (3) or (4), whose conversion has the object:
`weak_ptr<Base>(w.lock())`, or from the `tracked_ptr` the program already holds.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Base {
    virtual ~Base() = default;
};

struct Item : Base {
    int value = 7;
};

int main() {
    tracked_ptr item = make_tracked<Item>();
    weak_ptr weak = item;               // weak_ptr<Item>, a cell of its own
    weak_ptr<const Item> read = weak;   // the same cell, const added
    weak_ptr<Base> base = item;         // a base: from the strong pointer, another cell
    weak_ptr<Base> later = weak.lock();
    weak_ptr<Item> none;
    println("{} {} {} {}", weak.lock() == item, read.lock() == item, base.lock() == item,
            later.lock() == item);
    println("{}", none.expired());
}
```

Output:

```text
true true true true
true
```

## See also

- [operator=](operator_assign.md): assigns the pointer
- [lock](lock.md): the object as a `tracked_ptr`
- [sgcl::weak_ptr\<T\>](../weak_ptr.md)
