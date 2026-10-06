[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md) › [level](README.md)

# sgcl::compress::operator== (sgcl::compress::brotli::level)

```cpp
friend constexpr bool operator==(level a, level b) noexcept = default;
```

Compares two levels. `!=` is made from it by the compiler, and an `int` on either side converts to a level first.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the levels compared |

## Return value

`true` when the two levels have the same value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::brotli::options o;
    println("{}", o.level == compress::brotli::level::standard);
    println("{}", o.level == compress::brotli::level(5));
}
```

Output:

```text
true
false
```

## See also

- [value](value.md): the level as an `int`
- [sgcl::compress::brotli::level](README.md)
