[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::code_error

```cpp
#include "sgcl/txt/locale.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class code_error {
    public:
        enum class kind : uint8_t {
            currency,
            region,
            script,
        };
    };
}
```

`sgcl::txt::code_error` is why a text is not a code: of ISO 4217 for a [currency](../currency/README.md), three
ASCII letters; of ISO 3166 or UN M.49 for a [region](../region/README.md), two ASCII letters or three digits; of ISO
15924 for a [script_code](../script_code/README.md), four ASCII letters. It says which kind was asked for and the
byte the reading stopped on. A plain value of a few bytes; it lives anywhere.

## Member types

| Type | Definition |
|---|---|
| `kind` | the kind of code: `currency`, `region`, `script` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](code_error.md) | constructs the error of a kind of code at a byte |

#### Observers

| Function | Description |
|---|---|
| [offset](offset.md) | the byte the reading stopped on |
| [what](what.md) | the kind of code asked for |
| [message](message.md) | the error as a sentence |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto c = txt::currency::parse("zl");
    if (!c) {
        println("{} at {}", c.error().message(), c.error().offset());
    }
}
```

Output:

```text
not a currency code: three ASCII letters expected at 2
```

## See also

- [currency::parse](../currency/parse.md)
- [sgcl::txt](../README.md)
