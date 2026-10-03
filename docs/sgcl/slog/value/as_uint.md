[sgcl](../../README.md) › [slog](../README.md) › [value](../value.md)

# sgcl::slog::value::as_uint

```cpp
uint64_t as_uint() const;
```

Returns the value of a value of kind `uint64`: the unsigned integer given, as `uint64_t`. A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The unsigned integer given, as `uint64_t`.

## Complexity

Constant.

## Exceptions

`logic_error` when [type](type.md) is not `uint64`: `sgcl::slog::value::as_uint: a value of another kind`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "bytes", uint64_t(18446744073709551615u));
    println("{}", (*kept.records()[0].begin()).value().as_uint());
}
```

Output:

```text
18446744073709551615
```

## See also

- [type](type.md)
- [kind](../value-kind.md): `uint64`
- [sgcl::slog::value](../value.md)
