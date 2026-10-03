[sgcl](../../README.md) › [core](../README.md) › [move_only_function](../move_only_function.md)

# sgcl::move_only_function\<R(Args...)\>::operator()

```cpp
R operator()(Args... args) noexcept(Noexcept) requires (!Const);       // (1)
R operator()(Args... args) const noexcept(Noexcept) requires Const;    // (2)
```

Calls the callable with `args...`, forwarded, through `std::invoke`, and returns its result converted to `R`, or
nothing for an `R` of `void`. `Const` is whether the signature is `const`, `Noexcept` whether it is `noexcept`.

1. For a signature without `const`: called on a `move_only_function` that is not `const`, the callable as a `VF&`.
2. For a `const` signature: callable through a `const move_only_function&` as well, the callable as a `const VF&`.

Precondition: the `move_only_function` holds a callable. Calling an empty one is undefined, as with `std`; debug
builds assert.

## Parameters

| Parameter | Description |
|---|---|
| `args` | the arguments of the call |

## Return value

What the callable returns, as an `R`.

## Complexity

The call of the callable, plus one indirect call.

## Exceptions

What the callable throws; none for a `noexcept` signature, whose callables cannot throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    move_only_function<int(int)> counter = [n = 0](int step) mutable { return n += step; };
    counter(2);
    println("{}", counter(3));

    const move_only_function<int(int) const> square = [](int x) { return x * x; };
    println("{}", square(4));

    using Safe = move_only_function<int(int) noexcept>;
    println("{}", std::is_nothrow_invocable_v<Safe&, int>);
}
```

Output:

```text
5
16
true
```

## See also

- [operator bool](operator_bool.md): checks whether there is a callable to call
- [sgcl::move_only_function\<R(Args...)\>](../move_only_function.md)
