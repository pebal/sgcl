[sgcl](../../README.md) › [core](../README.md) › [any](README.md)

# sgcl::any::any

```cpp
any() noexcept = default;                                                              // (1)
any(const any& o) = default;                                                           // (2)
any(any&& o) noexcept = default;                                                       // (3)
template<class T, class VT = std::decay_t<T>>
any(T&& value) noexcept(std::is_nothrow_constructible_v<VT, T>);                       // (4)
template<class T, class... A, class VT = std::decay_t<T>>
requires std::is_constructible_v<VT, A...> && std::is_copy_constructible_v<VT>
explicit any(std::in_place_type_t<T>, A&&... a)                                        // (5)
    noexcept(std::is_nothrow_constructible_v<VT, A...>);
template<class T, class U, class... A, class VT = std::decay_t<T>>
requires std::is_constructible_v<VT, std::initializer_list<U>&, A...>
         && std::is_copy_constructible_v<VT>
explicit any(std::in_place_type_t<T>, std::initializer_list<U> il, A&&... a)           // (6)
    noexcept(std::is_nothrow_constructible_v<VT, std::initializer_list<U>&, A...>);
```

Constructs an `any`.

1. An empty `any`.
2. A copy of `o`: a copy of its value, if any. A value in a node is copied into a node of its own.
3. Takes the value of `o` over; `o` is empty after. A value in a node moves with its node, without a copy.
4. Holds `std::forward<T>(value)` as a `VT`. Takes part only when `VT` is copy constructible and neither `any` nor a
   `std::in_place_type_t`.
5. Holds a `VT` constructed from `a...`.
6. Holds a `VT` constructed from `il` and `a...`.

- (4–6) The value goes into the word when `VT` is a pointer word (`tracked_ptr`, `weak_ptr`), into the buffer when
  it is a small value that cannot hold a pointer, and into a managed node of its own otherwise
  ([any](README.md)).

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `any` to copy or to take the value from |
| `value` | the value to hold |
| `a` | the arguments the value is constructed from |
| `il` | the initializer list the value is constructed from |

## Complexity

Constant: one managed allocation for a value in a node (2, 4–6), none for the others.

## Exceptions

- (1), (3) None.
- (2) What the copy constructor of the held value throws.
- (4–6) What the constructor of `VT` throws; none when it is noexcept.

If an exception is thrown, no `any` is made.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    any empty;
    any number = 42;
    tracked_ptr node = make_tracked<Node>(7);
    any pointer = node;  // in the word
    any text(std::in_place_type<string>, "abc");
    any list(std::in_place_type<vector<int>>, {1, 2, 3});
    any copy = list;  // a node of its own
    any taken = std::move(number);

    println("{} {} {}", empty.has_value(), number.has_value(), taken.has_value());
    println("{} {}", any_cast<tracked_ptr<Node>&>(pointer)->value, any_cast<string&>(text));
    println("{} {}", any_cast<vector<int>&>(copy), any_cast<int>(taken));
}
```

Output:

```text
false false true
7 abc
[1, 2, 3] 42
```

## See also

- [operator=](operator_assign.md): assigns another `any` or a value
- [emplace](emplace.md): constructs a value in place
- [make_any](../make_any.md): an `any` with a value constructed in place
- [sgcl::any](README.md)
