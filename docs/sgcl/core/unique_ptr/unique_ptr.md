[sgcl](../../README.md) › [core](../README.md) › [unique_ptr](README.md)

# sgcl::unique_ptr\<T\>::unique_ptr

```cpp
unique_ptr() = default;                                                                                                      // (1)
unique_ptr(unique_ptr&&) noexcept = default;                                                                                 // (2)
constexpr unique_ptr(std::nullptr_t) noexcept;                                                                               // (3)
template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
unique_ptr(std::unique_ptr<U, deleter_type>&& p) noexcept;                                                                   // (4)
```

Constructs the owner.

1. An empty owner.
2. Takes the object of the source over; the source is empty after.
3. An empty owner.
4. Takes the object of a `unique_ptr<U>` over, `U*` convertible to `T*`: a base class, or `void`. The source is
   empty after.

There is no constructor from a raw pointer: an object enters a `unique_ptr` through
[make_tracked](../make_tracked.md) only.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the owner the object is taken from |

## Complexity

Constant.

## Exceptions

None.

## Notes

(4) is declared over the `std::unique_ptr` base of `unique_ptr<U>`, with the library's deleter (`deleter_type`):
it takes a `unique_ptr<U>` as it is.

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
    unique_ptr item = make_tracked<Item>();   // unique_ptr<Item>
    unique_ptr<Base> base = std::move(item);  // the Item, as its base
    unique_ptr<Item> none;
    unique_ptr<Item> null = nullptr;
    println("{} {} {} {}", bool(item), bool(base), bool(none), bool(null));
}
```

Output:

```text
false true false false
```

## See also

- [operator=](operator_assign.md): takes another object over
- [make_tracked](../make_tracked.md): creates a managed object
- [sgcl::unique_ptr\<T\>](README.md)
