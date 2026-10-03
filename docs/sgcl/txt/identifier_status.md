[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::identifier_status

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class identifier_status : uint8_t {
        restricted,
        allowed,
    };
}
```

The `Identifier_Status` property of [UTS #39](https://www.unicode.org/reports/tr39/), what
[identifier_status_of](identifier_status_of.md) answers: whether a code point is one the specification would let into
a name at all. `Allowed` is exactly the code points whose [identifier type](identifier_type.md) is `recommended` or
`inclusion`.

| Value | Description |
|---|---|
| `restricted` | not allowed in an identifier; the default, a code point nobody has argued for is not allowed |
| `allowed` | allowed in an identifier |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (char32_t c : {U'a', U'ł', U'ſ', U'_', U' '}) {
        bool allowed = txt::identifier_status_of(c) == txt::identifier_status::allowed;
        println("[{}] {}", c, allowed ? "allowed" : "restricted");
    }
}
```

Output:

```text
[a] allowed
[ł] allowed
[ſ] restricted
[_] allowed
[ ] restricted
```

## See also

- [identifier_status_of](identifier_status_of.md): the status of a code point
- [is_allowed_identifier](is_allowed_identifier.md): whether every code point of a text is allowed
- [identifier_type](identifier_type.md): why a code point is not allowed
- [txt](README.md)
