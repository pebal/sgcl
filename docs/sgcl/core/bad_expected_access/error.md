[sgcl](../../README.md) › [core](../README.md) › [bad_expected_access](../bad_expected_access.md)

# sgcl::bad_expected_access\<E\>::error

```cpp
/*(1)*/ const E& error() const& noexcept;
/*(2)*/ E& error() & noexcept;
/*(3)*/ const E&& error() const&& noexcept;
/*(4)*/ E&& error() && noexcept;
```

The error the exception carries: a reference into the managed object that holds it, alive for as long as the
exception or a copy of it is.

- (1–2) A reference to the error.
- (3–4) An rvalue reference to the error, from an rvalue exception.

## Parameters

None.

## Return value

A reference to the error.

## Complexity

Constant: one indirection through the root.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    expected<int, int> status = unexpected(503);
    try {
        status.value();
    } catch (const bad_expected_access<int>& e) {
        println("{}", e.error());
    }
}
```

Output:

```text
503
```

## See also

- [what](what.md): the error's message
- [sgcl::bad_expected_access\<E\>](../bad_expected_access.md)
