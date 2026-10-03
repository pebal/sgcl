[sgcl](../../README.md) › [core](../README.md) › [tracked_ptr](../tracked_ptr.md)

# sgcl::tracked_ptr\<T\>::tracked_ptr

```cpp
/*(1)*/ tracked_ptr() noexcept;
/*(2)*/ tracked_ptr(std::nullptr_t) noexcept;
/*(3)*/ template<class U, std::enable_if_t<std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit tracked_ptr(U* p) noexcept;
/*(4)*/ tracked_ptr(const tracked_ptr& p) noexcept;
/*(5)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(const tracked_ptr<U>& p) noexcept;
/*(6)*/ tracked_ptr(tracked_ptr&& p) noexcept;
/*(7)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(tracked_ptr<U>&& p) noexcept;
/*(8)*/ template<class U, std::enable_if_t<!std::is_same_v<U, T> && std::is_convertible_v<U*, element_type*>, int> = 0>
        tracked_ptr(const root_ptr<U>& r) noexcept;
/*(9)*/ template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(unique_ptr<U>&& u) noexcept;
/*(10)*/ tracked_ptr(const tracked_ptr& p, barrier::off_t) noexcept;
```

Constructs a pointer from one of the sources below.

1. A null pointer.
2. The same as (1).
3. The address of a managed object or of a part of it, a member or a base subobject: the alias keeps the whole
   object alive. Never an element of a container's buffer, never an object a `unique_ptr` owns; debug builds
   assert both.
4. A copy of `p`.
5. A copy of `p` converted to a base class, or to `void`: `U*` converts to `T*`.
6. A copy of `p`: a move is a copy, the source keeps its value.
7. The same as (5); the source keeps its value.
8. The pointer of a `root_ptr` of a derived class or of `T` made `const`. A `root_ptr<T>` converts by its own
   operator ([ptr](../root_ptr/ptr.md)).
9. The object of `u`, released from its owner: from then on the collector destroys it, when nothing reaches it any
   more. `u` is empty after.
10. A copy of `p` stored without the write barrier, for a word of an immutable node built in place
    ([store](store.md)).

## Parameters

| Parameter | Description |
|---|---|
| `p` | the address, or the pointer, to copy |
| `r` | the root whose pointer is copied |
| `u` | the owner the object is taken from |

## Complexity

Constant.

## Exceptions

None.

## Notes

Every constructor registers the calling thread with the collector on first contact (one thread-local load), stores
the word with the barrier and checks, in a debug build, that the pointer lives where the rules allow and addresses
what they allow. (10) is the exception: it asserts that the thread is registered already and stores the word
without the barrier.

A [slice](../slice.md) over unmanaged memory has no owner, and a thread that only makes such slices never touches
the collector: its null owner is made by a private constructor of the library's own, without the thread-local load,
and a value stored into that word later comes through an assignment whose caller, the slice, registers the thread
first.

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
    tracked_ptr item = make_tracked<Item>();  // from a unique_ptr: the collector's Item now
    tracked_ptr<Base> base = item;
    tracked_ptr alias(&item->value);          // tracked_ptr<int>, into the Item
    tracked_ptr<Item> none;
    tracked_ptr moved = std::move(item);
    println("{} {}", item == moved, none == nullptr);

    item = nullptr;
    moved = nullptr;
    base = nullptr;
    println("{}", *alias);  // the alias keeps the Item
}
```

Output:

```text
true true
7
```

## See also

- [operator=](operator_assign.md): assigns the pointer
- [make_tracked](../make_tracked.md): creates a managed object
- [sgcl::tracked_ptr\<T\>](../tracked_ptr.md)
