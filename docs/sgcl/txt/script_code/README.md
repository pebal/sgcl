[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::script_code

```cpp
#include "sgcl/txt/locale.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class script_code;
}
```

`sgcl::txt::script_code` is a script: a code of ISO 15924, four letters, in four bytes, packed as a
[locale](../locale/README.md) packs its script; not txt::script, the enumeration of the script property of Unicode
([script](../script.md)). Its [display_name](display_name.md) is its name in a language, from the optional headers
of the [display names](../names.md).

## Rules

- Four bytes, trivially copyable: it lives anywhere.
- A code the program writes is constructed and throws when it is not one; a code from outside is read by
  [parse](parse.md) into an `expected` with a [code_error](../code_error/README.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](script_code.md) | constructs none, or the one a code names |
| [parse](parse.md) | reads a code from outside the program (static) |

#### Observers

| Function | Description |
|---|---|
| [code](code.md) | the code: `"Latn"` |
| [display_name](display_name.md) | its name in a language: `"łacińskie"` |
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
    auto x = txt::script_code::parse("Latn");
    println("{} {}", x->code(), x->display_name(txt::locale("pl")));
}
```

Output:

```text
Latn łacińskie
```

## See also

- [display names](../names.md)
- [locale](../locale/README.md)
- [sgcl::txt](../README.md)
