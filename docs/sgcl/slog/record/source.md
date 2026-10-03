[sgcl](../../README.md) › [slog](../README.md) › [record](../record.md)

# sgcl::slog::record::source

```cpp
const std::source_location& source() const noexcept;
```

Returns where the call that made the record is, the [message](../message.md)'s place: there whether or not the
logger writes it, which [has_source](has_source.md) says.

## Parameters

None.

## Return value

The `std::source_location` of the call.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("here");
    const auto& where = kept.records()[0].source();
    println("line {} of {}", where.line(), where.function_name());
}
```

Output:

```text
line 8 of int main()
```

## See also

- [has_source](has_source.md)
- [options](../options.md): `source`
- [sgcl::slog::record](../record.md)
