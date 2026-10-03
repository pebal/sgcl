[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::keeps_dot

```cpp
constexpr bool keeps_dot() const noexcept;
```

Checks whether the language keeps the dot above an `i` or a `j` under an accent: Lithuanian.

## Parameters

None.

## Return value

`true` for Lithuanian.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::locale::lithuanian().keeps_dot(), txt::locale::turkish().keeps_dot());
}
```

Output:

```text
true false
```

## See also

- [dotted_i](dotted_i.md)
- [sgcl::txt::locale](README.md)
