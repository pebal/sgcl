[sgcl](../../README.md) › [async](../README.md) › [promise](../promise.md)

# sgcl::async::promise\<T\>::operator=

```cpp
promise& operator=(const promise& other) noexcept;    // (1)
promise& operator=(promise&& other) noexcept;         // (2)
```

Makes the handle one of the promise `other` refers to. The promise this handle referred to before is not touched: it
stays as it was, set or not, and lives on while another handle holds it.

1. A copy of the handle.
2. The same as (1): `other` still refers to the promise after.

The assignments of `promise<void>` are the same.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle of the promise to refer to |

## Return value

`*this`.

## Complexity

Constant: a store of one tracked word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::promise<int> first, second;
    async::promise<int> current = first;
    current.set_value(1);

    current = second;  // a fresh promise to set
    current.set_value(2);
    println("{} {}", first.result(), second.result());
}
```

Output:

```text
1 2
```

## See also

- [(constructor)](promise.md): makes a promise, or another handle of one
- [operator==, operator!=](operator_cmp.md): whether two handles are of the same promise
- [sgcl::async::promise\<T\>](../promise.md)
