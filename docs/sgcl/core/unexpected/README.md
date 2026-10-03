[sgcl](../../README.md) › [core](../README.md)

# sgcl::unexpected\<E\>

```cpp
#include "sgcl/core/expected.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class E>
    class unexpected;
}
```

`sgcl::unexpected<E>` is the error of an [expected](../expected/README.md), wrapped: what tells the constructor and the
assignment of an `expected` that they are getting an error, not a value, as `std::unexpected` (C++23) tells
`std::expected`. `return unexpected("no user");` from a function that returns an `expected<T, string>` returns the
error; the deduction guide makes `unexpected("text")` an `unexpected<const char*>`, which converts to the error
type. The library is C++20, so the class is its own, with the interface of the `std` one.

## Rules

- An `unexpected` holds its error as a member: it lives where its error may, an error with a `tracked_ptr` inside on
  a stack or inside a managed object ([The rules](../README.md#the-rules), 1). It is a temporary in most programs,
  made to be passed to an `expected`.
- Thread safety is that of its error.

## Template parameters

| Parameter | Description |
|---|---|
| `E` | The type of the error: an object type that is not an array, not `const` or `volatile`, and not an `unexpected`. |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](unexpected.md) | constructs the `unexpected` from an error |
| `(destructor)` | destroys the error |
| `operator=` | assigns the error of another `unexpected`, copied or moved |
| [error](error.md) | the error |
| [swap](swap.md) | swaps the errors of two `unexpected` objects |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares the errors |
| [swap](swap2.md) | swaps the errors of two `unexpected` objects |

## Deduction guides

```cpp
template<class E>
unexpected(E) -> unexpected<E>;
```

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<int, string> divide(int a, int b) {
    if (b == 0) {
        return unexpected("division by zero");  // an unexpected<const char*>: converted
    }
    return a / b;
}

int main() {
    println("{} {}", *divide(6, 3), divide(1, 0).error());
}
```

Output:

```text
2 division by zero
```

## See also

- [expected](../expected/README.md): a value or an error
- [bad_expected_access](../bad_expected_access/README.md): the exception that carries the error
