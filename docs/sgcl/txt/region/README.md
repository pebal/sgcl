[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::region

```cpp
#include "sgcl/txt/locale.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class region;
}
```

`sgcl::txt::region` is a region: a code of ISO 3166 (two letters) or of UN M.49 (three digits), in two bytes, packed
as a [locale](../locale/README.md) packs its region. Its [display_name](display_name.md) is its name in a language,
from the optional headers of the [display names](../names.md).

## Rules

- Two bytes, trivially copyable: it lives anywhere.
- A code the program writes is constructed and throws when it is not one; a code from outside is read by
  [parse](parse.md) into an `expected` with a [code_error](../code_error/README.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](region.md) | constructs none, or the one a code names |
| [parse](parse.md) | reads a code from outside the program (static) |

#### Observers

| Function | Description |
|---|---|
| [code](code.md) | the code: `"PL"` |
| [display_name](display_name.md) | its name in a language: `"Niemcy"` |
| [operator==](operator_cmp.md) | checks whether two codes are one |
| [operator bool](operator_bool.md) | checks whether it holds a code |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    auto x = txt::region::parse("DE");
    println("{} {}", x->code(), x->display_name(txt::locale("pl")));
}
```

Output:

```text
DE Niemcy
```

## See also

- [display names](../names.md)
- [locale](../locale/README.md)
- [sgcl::txt](../README.md)
