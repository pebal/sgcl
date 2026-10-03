[sgcl](../../README.md) › [slog](../README.md) › [value](README.md)

# sgcl::slog::value::as_duration

```cpp
sgcl::duration as_duration() const;
```

Returns the value of a value of kind `duration`: the `duration` or `std::chrono` duration given, as a [duration](../../core/duration/README.md). A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The `duration` or `std::chrono` duration given, as a [duration](../../core/duration/README.md).

## Complexity

Constant.

## Exceptions

`logic_error` when [type](type.md) is not `duration`: `sgcl::slog::value::as_duration: a value of another kind`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "took", std::chrono::milliseconds(1500));
    println("{}", (*kept.records()[0].begin()).value().as_duration());
}
```

Output:

```text
1.5s
```

## See also

- [type](type.md)
- [kind](../value-kind.md): `duration`
- [sgcl::slog::value](README.md)
