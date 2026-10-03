[sgcl](../../README.md) › [core](../README.md) › [function](README.md)

# sgcl::function\<R(Args...)\>::operator=

```cpp
function& operator=(const function& o);                                         // (1)
function& operator=(function&& o) noexcept;                                     // (2)
function& operator=(std::nullptr_t) noexcept;                                   // (3)
template<class F, class VF = std::decay_t<F>>
requires std::is_copy_constructible_v<VF>
function& operator=(F&& f) noexcept(std::is_nothrow_constructible_v<VF, F>);    // (4)
template<class F>
function& operator=(std::reference_wrapper<F> f) noexcept;                      // (5)
```

Replaces the callable held.

1. A copy of the callable of `o`, if any.
2. The callable of `o`, taken over; `o` is empty after.
3. Nothing: the `function` is empty after.
4. `std::forward<F>(f)`, placed as the [constructor](function.md) places it. Takes part when the constructor does.
5. `f` itself, a reference to a callable kept elsewhere: one raw pointer in the buffer, no allocation, no copy of the
   callable. Takes part only when the callable is callable with `Args...` and gives what converts to `R`.

- (1), (2), (4), (5) The new callable is made first, in a temporary `function`, and swapped in.

The old callable is destroyed then, on the calling thread; a closure in a node goes with its node, which is left to
the collector.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `function` to copy or to take the callable from |
| `f` | the callable to hold, or a reference to it |

## Return value

`*this`.

## Complexity

Constant: one managed allocation for a new closure in a node (1, 4), none for the others.

## Exceptions

- (1) What the copy constructor of the callable of `o` throws.
- (2–3), (5) None.
- (4) What the constructor of `VF` throws; none when it is noexcept.

If an exception is thrown, the `function` is as it was.

## Notes

With (5) the `function` keeps a raw pointer to the callable, as `std::function` keeps it: the callable must outlive
the `function`, and the collector does not see that pointer as a reference to it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

struct Counter {
    int calls = 0;
    void operator()() {
        ++calls;
    }
};

int main() {
    function<void()> f = [] { println("first"); };
    f();
    f = [] { println("second"); };  // the first closure destroyed
    f();

    Counter counter;
    f = std::ref(counter);  // a reference: the calls reach counter itself
    f();
    f();
    println("{}", counter.calls);

    f = nullptr;
    println("{}", bool(f));
}
```

Output:

```text
first
second
2
false
```

## See also

- [swap](swap.md): swaps the callables of two `function` objects
- [(constructor)](function.md): where a callable goes
- [sgcl::function\<R(Args...)\>](README.md)
