[sgcl](../../README.md) › [core](../README.md) › [move_only_function](README.md)

# sgcl::operator== (sgcl::move_only_function)

```cpp
friend bool operator==(const move_only_function& f, std::nullptr_t) noexcept;
```

Checks whether `f` is empty: `!f`. A hidden friend, found by the argument's type alone; `nullptr == f`,
`f != nullptr` and `nullptr != f` are rewritten to it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the `move_only_function` to check |

## Return value

`true` when `f` holds no callable, `false` otherwise.

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
    move_only_function<void()> f;
    println("{}", f == nullptr);
    f = [] {};
    println("{}", f != nullptr);
}
```

Output:

```text
true
true
```

## See also

- [operator bool](operator_bool.md): the same question
- [sgcl::move_only_function\<R(Args...)\>](README.md)
