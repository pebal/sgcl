[sgcl](../../README.md) › [core](../README.md) › [move_only_function](../move_only_function.md)

# sgcl::move_only_function\<R(Args...)\>::operator=

```cpp
/*(1)*/ move_only_function& operator=(move_only_function&& o) noexcept;
/*(2)*/ move_only_function& operator=(const move_only_function&) = delete;
/*(3)*/ move_only_function& operator=(std::nullptr_t) noexcept;
/*(4)*/ template<class F>
        requires std::is_constructible_v<move_only_function, F>
        move_only_function& operator=(F&& f)
            noexcept(std::is_nothrow_constructible_v<move_only_function, F>);
```

Replaces the callable held.

1. The callable of `o`, taken over; `o` is empty after.
2. Not copyable.
3. Nothing: the `move_only_function` is empty after.
4. `std::forward<F>(f)`, placed as the [constructor](move_only_function.md) places it.

- (1), (4) The new callable is made first, in a temporary, and swapped in.

The old callable is destroyed then, on the calling thread; a closure in a node goes with its node, which is left to
the collector.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `move_only_function` to take the callable from |
| `f` | the callable to hold |

## Return value

`*this`.

## Complexity

Constant: one managed allocation for a new callable in a node (4), none for the others.

## Exceptions

- (1), (3) None.
- (4) What the constructor of the callable throws; none when it is noexcept.

If an exception is thrown, the `move_only_function` is as it was.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <memory>

using namespace sgcl;

int main() {
    move_only_function<void()> task = [] { println("first"); };
    task();
    task = [p = std::make_unique<int>(2)] { println("second, {}", *p); };
    task();
    task = nullptr;
    println("{}", bool(task));
}
```

Output:

```text
first
second, 2
false
```

## See also

- [swap](swap.md): swaps the callables of two `move_only_function` objects
- [sgcl::move_only_function\<R(Args...)\>](../move_only_function.md)
