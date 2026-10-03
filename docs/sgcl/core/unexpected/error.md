[sgcl](../../README.md) › [core](../README.md) › [unexpected](../unexpected.md)

# sgcl::unexpected\<E\>::error

```cpp
/*(1)*/ const E& error() const& noexcept;
/*(2)*/ E& error() & noexcept;
/*(3)*/ const E&& error() const&& noexcept;
/*(4)*/ E&& error() && noexcept;
```

The error the `unexpected` holds.

- (1–2) A reference to the error.
- (3–4) An rvalue reference to the error, from an rvalue `unexpected`.

## Parameters

None.

## Return value

A reference to the error.

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
    unexpected<string> u(string("broken"));
    u.error() = "repaired";
    println("{}", u.error());
}
```

Output:

```text
repaired
```

## See also

- [(constructor)](unexpected.md): constructs the `unexpected`
- [sgcl::unexpected\<E\>](../unexpected.md)
