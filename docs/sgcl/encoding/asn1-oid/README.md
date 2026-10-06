[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md)

# sgcl::encoding::asn1::oid

```cpp
#include "sgcl/encoding/asn1.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class asn1 {
    public:
        class oid;
    };
}
```

`sgcl::encoding::asn1::oid` is an OBJECT IDENTIFIER (X.660): a path of numbers, the arcs, that names an algorithm,
an attribute or an extension once and for all — `1.2.840.113549.1.1.1` is RSA, `2.5.4.3` a common name. It keeps
the DER of its arcs inline, at most 63 bytes, so it is a plain value of 64 bytes with no pointer: a constant, a
global, a key of a table, compared and hashed by its bytes. Go's `asn1.ObjectIdentifier` is a slice of `int`s,
an arc at most 31 bits; an arc here is of any size (`2.25.<a UUID>` has 128 bits).

## Rules

- **A literal is checked at compile time**: `asn1::oid rsa("1.2.840.113549.1.1.1");` converts the literal
  in a constant expression, and one that is not an identifier is an error of the compiler. A text from outside is
  [parse](parse.md)d; `asn1::oid(text)` with a [string](../../core/string/README.md) throws what `parse` returns.
- **What an identifier is** (X.660): two arcs at least, the first 0, 1 or 2, the second below 40 under 0 and 1;
  each arc written in decimal without a leading zero. Its DER is at most 63 bytes, which holds any identifier in use
  (`2.25.<a UUID>` takes 20).
- **The order is arc by arc**, a prefix first (`1.2` < `1.2.3` < `1.10`), which is the order of the DER bytes;
  [starts_with](starts_with.md) asks whether one names a subtree of another.
- `oid` holds no tracked word: it lives anywhere. `println("{}", id)` writes it dotted.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](asn1-oid.md) | none, of a literal, of a text, of arcs |
| [parse](parse.md) | an identifier of a dotted text (static) |
| [size](size.md) | the count of arcs |
| [arc](arc.md) | an arc by its place |
| [starts_with](starts_with.md) | whether another is a prefix of it |
| [to_string](to_string.md) | the identifier dotted |
| [operator bool](operator_bool.md) | whether it has arcs |
| [hash](hash.md) | a hash of its bytes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | equality, and the order arc by arc |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    constexpr encoding::asn1::oid ec_public_key("1.2.840.10045.2.1");
    auto read = encoding::asn1::oid::parse("1.2.840.10045.3.1.7");
    if (!read) {
        println(read.error().message());
        return 1;
    }
    println("{} has {} arcs", *read, read->size());
    println(read->starts_with(encoding::asn1::oid("1.2.840.10045")));
    println(*read < ec_public_key);
    println(encoding::asn1::oid::parse("1.2.040").error().message());
}
```

Output:

```text
1.2.840.10045.3.1.7 has 7 arcs
true
false
offset 4: not an OBJECT IDENTIFIER
```

## See also

- [asn1](../asn1/README.md): an element of DER, [object_identifier](../asn1/object_identifier.md) and [as_oid](../asn1/as_oid.md)
- [sgcl::encoding](../README.md)
