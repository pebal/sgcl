[sgcl](../README.md) › [encoding](README.md) › [asn1](asn1/README.md)

# sgcl::encoding::asn1::tag_class

```cpp
#include "sgcl/encoding/asn1.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class asn1 {
    public:
        enum class tag_class : uint8_t {
            universal,
            application,
            context_specific,
            private_use
        };
    };
}
```

`sgcl::encoding::asn1::tag_class` is the class of a tag (X.680 §8.1), the top two bits of its first byte, which with
the tag's number names an element's type: [cls](asn1/cls.md) gives it, [explicit_tag](asn1/explicit_tag.md),
[implicit_tag](asn1/implicit_tag.md) and [raw](asn1/raw.md) take it. Go's `asn1.ClassUniversal` and the rest.

| Value | Description |
|---|---|
| `universal` | the types X.680 names: INTEGER, SEQUENCE, UTF8String… ([asn1::type](asn1-type.md)); 0 |
| `application` | a type a protocol names for itself: LDAP's messages; 1 |
| `context_specific` | `[0]`, `[1]`…: the components of one SEQUENCE told apart; 2 |
| `private_use` | a type of one organisation's; 3 |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 bind = encoding::asn1::implicit_tag(0, encoding::asn1::sequence({}),
                                                       encoding::asn1::tag_class::application);
    println("{} {}", int(bind.cls()), bind.cls() == encoding::asn1::tag_class::application);
    print(bind.to_string());
}
```

Output:

```text
1 true
[APPLICATION 0]
```

## See also

- [cls](asn1/cls.md), [tag](asn1/tag.md)
- [sgcl::encoding::asn1](asn1/README.md)
