[sgcl](../../README.md) › [core](../README.md) › [function](README.md)

# sgcl::function\<R(Args...)\>::operator()

```cpp
R operator()(Args... args) const;
```

Calls the callable with `args...`, forwarded, through `std::invoke`, and returns its result converted to `R`, or
nothing for an `R` of `void`. The callable is called as an lvalue that is not `const`, as `std::function` calls it:
a closure with a `mutable` lambda's state changes it, through a `const function&` as well.

## Parameters

| Parameter | Description |
|---|---|
| `args` | the arguments of the call |

## Return value

What the callable returns, as an `R`.

## Complexity

The call of the callable, plus one indirect call.

## Exceptions

`bad_function_call` when the `function` is empty, and what the callable throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    tracked_ptr node = make_tracked<Node>(10);
    function<int(int)> add = [node](int x) { return node->value + x; };
    println("{}", add(5));

    function<int()> next = [n = 0]() mutable { return ++n; };
    next();
    println("{}", next());

    function<void()> empty;
    try {
        empty();
    } catch (const bad_function_call&) {
        println("empty");
    }
}
```

Output:

```text
15
2
empty
```

## See also

- [operator bool](operator_bool.md): checks whether there is a callable to call
- [sgcl::function\<R(Args...)\>](README.md)
