[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::as_int, as_bool

```cpp
optional<int64_t> as_int() const noexcept;    // (1)
optional<bool> as_bool() const noexcept;      // (2)
```

1. An INTEGER: decimal with an optional sign (PRIORITY, SEQUENCE, PERCENT-COMPLETE).
2. A BOOLEAN: `TRUE` or `FALSE` in any case.

## Parameters

None.

## Return value

The value, or `nullopt` for one that does not read.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {} {}", encoding::content_line("PRIORITY", "1").as_int(), encoding::content_line("X", "true").as_bool(),
            encoding::content_line("X", "yes").as_bool());
}
```

Output:

```text
1 true nullopt
```

## See also

- [as_date](as_date.md)
- [sgcl::encoding::content_line](README.md)
