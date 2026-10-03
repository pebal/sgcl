[sgcl](../../README.md) › [core](../README.md) › [move_only_function](README.md)

# sgcl::swap (sgcl::move_only_function)

```cpp
friend void swap(move_only_function& l, move_only_function& r) noexcept;
```

Swaps the callables of `l` and `r`: `l.swap(r)` ([swap](swap.md)). A hidden friend: found by the argument's type
alone, by `swap(a, b)` written without a namespace and by the `using std::swap; swap(a, b);` of generic code.

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the `move_only_function` objects to swap |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    move_only_function<int()> a = [] { return 1; };
    move_only_function<int()> b;
    swap(a, b);
    println("{} {}", bool(a), b());
}
```

Output:

```text
false 1
```

## See also

- [swap](swap.md): the member function
- [sgcl::move_only_function\<R(Args...)\>](README.md)
