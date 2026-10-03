[sgcl](../../README.md) › [slog](../README.md) › [value](README.md)

# sgcl::slog::value::as_double

```cpp
double as_double() const;
```

Returns the value of a value of kind `float64`: the `float` or `double` given, as `double`. A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The `float` or `double` given, as `double`.

## Complexity

Constant.

## Exceptions

`logic_error` when [type](type.md) is not `float64`: `sgcl::slog::value::as_double: a value of another kind`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "ratio", 0.25f);
    println("{}", (*kept.records()[0].begin()).value().as_double());
}
```

Output:

```text
0.25
```

## See also

- [type](type.md)
- [kind](../value-kind.md): `float64`
- [sgcl::slog::value](README.md)
