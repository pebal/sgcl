[sgcl](../../README.md) › [core](../README.md) › [function](../function.md)

# sgcl::function\<R(Args...)\>::function

```cpp
/*(1)*/ function() noexcept = default;
/*(2)*/ function(std::nullptr_t) noexcept;
/*(3)*/ function(const function& o) = default;
/*(4)*/ function(function&& o) noexcept = default;
/*(5)*/ template<class F, class VF = std::decay_t<F>>
        requires std::is_copy_constructible_v<VF>
        function(F&& f) noexcept(std::is_nothrow_constructible_v<VF, F>);
```

Constructs a `function`.

1. An empty `function`.
2. An empty `function`.
3. A copy of `o`: a copy of its callable, if any. A closure in a node is copied into a node of its own.
4. Takes the callable of `o` over; `o` is empty after. A closure in a node moves with its node, without a copy.
5. Holds `std::forward<F>(f)` as a `VF`: in the buffer when it is a small value that cannot hold a pointer, in a
   managed node of its own otherwise ([function](../function.md)). A null function pointer, a null member pointer
   and an empty function of either library (`std::function`, `sgcl::function`) make an empty `function`. Takes part
   only when `VF` is not this `function` and an lvalue `VF` is callable with `Args...` and gives what converts to
   `R`.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `function` to copy or to take the callable from |
| `f` | the callable to hold |

## Complexity

Constant: one managed allocation for a closure in a node (3, 5), none for the others.

## Exceptions

- (1–2), (4) None.
- (3) What the copy constructor of the callable throws.
- (5) What the constructor of `VF` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int twice(int x) {
    return x * 2;
}

int main() {
    function<int(int)> empty;
    function<int(int)> from_pointer = twice;  // in the buffer
    tracked_ptr node = make_tracked<Node>(3);
    function<int(int)> capturing = [node](int x) { return x * node->value; };  // in a node
    function deduced = [](int x) { return x + 1; };  // function<int(int)>
    function<int(int)> copy = capturing;  // a node of its own

    int (*null)(int) = nullptr;
    function<int(int)> from_null = null;
    println("{} {}", bool(empty), bool(from_null));
    println("{} {} {} {}", from_pointer(5), capturing(5), deduced(5), copy(5));
}
```

Output:

```text
false false
10 15 6 15
```

## See also

- [operator=](operator_assign.md): assigns another `function`, a callable or `nullptr`
- [move_only_function](../move_only_function.md): a callable that need not be copyable
- [sgcl::function\<R(Args...)\>](../function.md)
