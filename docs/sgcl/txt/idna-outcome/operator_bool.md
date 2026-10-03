[sgcl](../../README.md) › [txt](../README.md) › [idna](../idna.md) › [outcome](../idna-outcome.md)

# sgcl::txt::idna::outcome::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether nothing was wrong with the name: `reason.rule == error::none`.

## Parameters

None.

## Return value

`true` when the name broke no rule.

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
    if (auto made = txt::idna::unicode_form("xn--bcher-kva.de")) {
        println("{}", made.text);
    }
    println("{}", bool(txt::idna::unicode_form("a..c")));
}
```

Output:

```text
bücher.de
false
```

## See also

- [sgcl::txt::idna::outcome](../idna-outcome.md)
