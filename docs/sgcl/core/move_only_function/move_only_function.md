[sgcl](../../README.md) › [core](../README.md) › [move_only_function](../move_only_function.md)

# sgcl::move_only_function\<R(Args...)\>::move_only_function

```cpp
move_only_function() noexcept = default;                                                       // (1)
move_only_function(std::nullptr_t) noexcept;                                                   // (2)
move_only_function(move_only_function&& o) noexcept = default;                                 // (3)
move_only_function(const move_only_function&) = delete;                                        // (4)
template<class F, class VF = std::decay_t<F>>
requires std::is_constructible_v<VF, F>
move_only_function(F&& f) noexcept(std::is_nothrow_constructible_v<VF, F>);                    // (5)
template<class T, class... A, class VF = std::decay_t<T>>
requires std::is_constructible_v<VF, A...>
explicit move_only_function(std::in_place_type_t<T>, A&&... a)                                 // (6)
    noexcept(std::is_nothrow_constructible_v<VF, A...>);
template<class T, class U, class... A, class VF = std::decay_t<T>>
requires std::is_constructible_v<VF, std::initializer_list<U>&, A...>
explicit move_only_function(std::in_place_type_t<T>, std::initializer_list<U> il, A&&... a)    // (7)
    noexcept(std::is_nothrow_constructible_v<VF, std::initializer_list<U>&, A...>);
```

Constructs a `move_only_function`.

1. An empty one.
2. An empty one.
3. Takes the callable of `o` over; `o` is empty after. A callable in a node moves with its node.
4. Not copyable: the callable has one owner.
5. Holds `std::forward<F>(f)` as a `VF`, in the buffer when it is a small value that cannot hold a pointer, in a
   managed node of its own otherwise. A null function pointer, a null member pointer, an empty `move_only_function`
   of any signature and an empty function of either library make an empty one. Takes part only when `VF` is not
   this class nor an `in_place_type_t`.
6. Holds a `VF` constructed from `a...`.
7. Holds a `VF` constructed from `il` and `a...`.

- (5–7) Take part only when `VF` is callable as the signature says: as a `const VF&` for a `const` signature, else
  as a `VF&`, with `Args...`, giving what converts to `R`, and without throwing for a `noexcept` signature.

The constructors are declared in a base class of the library and brought in by a using-declaration; (1) and (3) are
those of the class.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `move_only_function` to take the callable from |
| `f` | the callable to hold |
| `a` | the arguments the callable is constructed from |
| `il` | the initializer list the callable is constructed from |

## Complexity

Constant: one managed allocation for a callable in a node (5–7), none for the others.

## Exceptions

- (1–3) None.
- (5–7) What the constructor of `VF` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>
#include <utility>

using namespace sgcl;

struct Adder {
    int base;
    int operator()(int x) const {
        return base + x;
    }
};

int main() {
    move_only_function<int()> owning = [p = std::make_unique<int>(5)] { return *p; };
    move_only_function<int(int) const> in_place(std::in_place_type<Adder>, 10);
    move_only_function<int()> taken = std::move(owning);

    function<void()> empty;
    move_only_function<void()> from_empty = empty;
    println("{} {} {} {}", taken(), in_place(1), bool(owning), bool(from_empty));
}
```

Output:

```text
5 11 false false
```

## See also

- [operator=](operator_assign.md): assigns another `move_only_function`, a callable or `nullptr`
- [function](../function.md): the copyable one
- [sgcl::move_only_function\<R(Args...)\>](../move_only_function.md)
