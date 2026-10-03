[sgcl](../../README.md) › [slog](../README.md) › [value](README.md)

# sgcl::slog::value::as_bool

```cpp
bool as_bool() const;
```

Returns the value of a value of kind `boolean`: the `bool` given. A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The `bool` given.

## Complexity

Constant.

## Exceptions

`logic_error` when [type](type.md) is not `boolean`: `sgcl::slog::value::as_bool: a value of another kind`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "tls", true);
    println("{}", (*kept.records()[0].begin()).value().as_bool());
}
```

Output:

```text
true
```

## See also

- [type](type.md)
- [kind](../value-kind.md): `boolean`
- [sgcl::slog::value](README.md)
