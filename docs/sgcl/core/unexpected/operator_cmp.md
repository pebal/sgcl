[sgcl](../../README.md) › [core](../README.md) › [unexpected](README.md)

# sgcl::operator== (sgcl::unexpected)

```cpp
template<class E2>
friend bool operator==(const unexpected& x, const unexpected<E2>& y)
    noexcept(noexcept(bool(x.error() == y.error())));
```

Compares the errors: `x.error() == y.error()`. A hidden friend, found by the argument's type alone; `!=` is its
negation, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `x`, `y` | the `unexpected` objects to compare |

## Return value

`true` when the errors are equal, `false` otherwise.

## Complexity

One comparison of the errors.

## Exceptions

What the comparison of the errors throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    unexpected<string> a(string("timeout"));
    println("{} {}", a == unexpected("timeout"), a != unexpected("refused"));
}
```

Output:

```text
true true
```

## See also

- [operator==](../expected/operator_cmp.md): an `expected` compared with an `unexpected`
- [sgcl::unexpected\<E\>](README.md)
