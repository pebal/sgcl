[sgcl](../../README.md) › [core](../README.md) › [bad_expected_access](../bad_expected_access.md)

# sgcl::bad_expected_access\<E\>::bad_expected_access

```cpp
/*(1)*/ explicit bad_expected_access(E e) noexcept(std::is_nothrow_move_constructible_v<E>);
/*(2)*/ bad_expected_access(const bad_expected_access& o) noexcept;
/*(3)*/ bad_expected_access(bad_expected_access&& o) noexcept;
```

1. The exception with the error `e`, moved into a managed object of its own under a root.
2. A copy: the same error, shared through the root. The text of `what()` is the copy's own, made again when asked.
3. The same, the root moved.

An `expected` constructs the exception itself: a program constructs one only to throw it as an `expected` would.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |
| `o` | the exception to copy or to move from |

## Complexity

(1) One managed allocation and the move of `E`. (2–3) Constant.

## Exceptions

- (1) What the move constructor of `E` throws; none when it is noexcept.
- (2–3) None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    try {
        throw bad_expected_access<string>("thrown by hand");
    } catch (const bad_expected_access<void>& e) {  // the base catches every error type
        println("{}", e.what());
    }
}
```

Output:

```text
bad access to sgcl::expected without a value
```

## See also

- [error](error.md): the error
- [sgcl::bad_expected_access\<E\>](../bad_expected_access.md)
