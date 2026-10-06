[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::asn1

```cpp
#include "sgcl/encoding/asn1.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class asn1;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::asn1` is one element of ASN.1's Basic and Distinguished Encoding Rules
([X.690](https://www.itu.int/rec/T-REC-X.690)): a tag and its content, which is bytes or more elements — what
certificates, keys, CMS and PKCS #12, LDAP, SNMP and Kerberos are written in. One type is read and written, as
[json](../json/README.md) is one value: [parse](parse.md) gives an element whose elements are walked by
[operator\[\]](operator_at.md) or [an iterator](begin.md) and whose value is taken by the `as_*` functions, and
[sequence](sequence.md), [integer](integer.md) and the rest make one, whose [bytes](bytes.md) are its DER.

An element is a view of 32 bytes into the bytes it was read from or made into, immutable: a copy is a copy of the
view, and the bytes live as long as an element of them does. Go's `encoding/asn1` reads into a struct through its
tags (`asn1:"explicit,tag:0"`) by reflection; here a program walks the element, `cert[0][6]`, with no struct to
declare, and asks each part for the value it expects. What Go's `cryptobyte` does with a cursor and a check after
every step, `parse` does once: the structure and the content of every universal type are checked when the bytes are
read, and a walk after it cannot fail; only the value asked of the wrong type is `nullopt`.

## Rules

- **DER by default, BER when asked**: [parse](parse.md) of DER takes exactly what X.690 §10–11 allows — a length
  in its shortest form, a BOOLEAN of `00` or `FF`, an INTEGER in its shortest form, a BIT STRING whose unused bits
  are zero, the one form of each time, strings in one piece. With [asn1::ber](../asn1-options.md) it takes BER's
  forms as well: the indefinite length, strings in pieces, lengths in a longer form than they need (Active Directory
  writes four bytes always), a BOOLEAN of any byte, the times without their seconds or with an offset.
- **BER is turned into DER's form once**: an input with an indefinite length or a string in pieces is written
  again, definite, into a buffer of its own, which its elements view; an input with neither is viewed where it
  lies. The bytes of a CMS that OpenSSL streams come out as the DER `openssl cms -cmsout` writes of it. A
  constructed element of a class other than universal (CMS's `[0] IMPLICIT OCTET STRING` in pieces) stays
  constructed — its type is the schema's, not the encoding's — and [as_bytes](as_bytes.md) joins its pieces.
- **An implicit tag is read as the type asked**: an element of a class other than universal and primitive
  (`[1] IMPLICIT INTEGER`) gives whatever `as_*` is asked of it, its content held to that type's rules. An
  explicit tag is a constructed element; its value is its first element, `e[0]`.
- **What is made is DER**: an INTEGER in its shortest form, a SET in DER's order (X.690 §11.6), the bits past a
  BIT STRING's length zero, a time in UTC. A component left out is [asn1()](asn1.md): [sequence](sequence.md) and
  [set](set.md) skip it, so an OPTIONAL component is a condition in the list. A string outside its character set,
  a UTCTime outside 1950–2049, a tag number past 2^28 are `invalid_argument`: a mistake in the program throws, a
  mistake in the input is an [error](../error/README.md).
- **Where Go and X.690 differ, X.690 is kept** and the tests name each case against Go's `encoding/asn1`: DER's
  UTCTime has its seconds and a `Z` (Go reads `YYMMDDhhmmZ` and offsets), GeneralizedTime ends in `Z`, a
  PrintableString has no `*` or `&`, an arc of an OBJECT IDENTIFIER may pass 31 bits (`2.25.<UUID>` has 128); a time
  after 2262 is the last instant a [datetime](../../time/datetime/README.md) holds.
- **The limits**: elements nested deeper than `max_depth` (512) are `depth_limit`, an element of a stream longer
  than `max_size` (64 MiB) is `limit_exceeded` before it is held ([asn1::options](../asn1-options.md)); the walk of
  a parse and [to_string](to_string.md) never recurse.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `asn1.Unmarshal(der, &v)` | [asn1::parse(der)](parse.md), then `e[0].as_int()`, `e[1].as_oid()`… |
| `asn1.Marshal(v)` | [asn1::sequence({...})](sequence.md) of [integer](integer.md), [utf8_string](utf8_string.md)…, then [bytes()](bytes.md) |
| `asn1.RawValue` | the element itself: [cls](cls.md), [tag](tag.md), [constructed](constructed.md), [content](content.md), [bytes](bytes.md) |
| `asn1.ObjectIdentifier` | [asn1::oid](../asn1-oid/README.md), a plain value; a literal checked at compile time |
| `asn1.BitString` | [asn1::bits](../asn1-bits/README.md) |
| `asn1:"explicit,tag:0"`, `asn1:"tag:1"` | [explicit_tag](explicit_tag.md), [implicit_tag](implicit_tag.md); read by `e[0]` and `as_*` |
| `asn1:"set"` | [set](set.md), sorted as DER sorts |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `tag_class` | the class of a tag: [asn1::tag_class](../asn1-tag_class.md) |
| `type` | the universal types by their numbers: [asn1::type](../asn1-type.md) |
| `oid` | an OBJECT IDENTIFIER: [asn1::oid](../asn1-oid/README.md) |
| `bits` | a BIT STRING read: [asn1::bits](../asn1-bits/README.md) |
| `options` | what a parse accepts: [asn1::options](../asn1-options.md) |
| `iterator` | a forward iterator over the elements inside, whose `*` makes the element |

## Member objects

| Object | Description |
|---|---|
| `der` | the [options](../asn1-options.md) of DER, the defaults |
| `ber` | the [options](../asn1-options.md) of BER: `ber` set |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](asn1.md) | no element |
| [parse, async_parse](parse.md) | one element of bytes or of a stream (static) |

#### Making one

| Function | Description |
|---|---|
| [boolean](boolean.md) | a BOOLEAN (static) |
| [integer](integer.md) | an INTEGER of any integral type or a [big_integer](../../math/big_integer/README.md) (static) |
| [enumerated](enumerated.md) | an ENUMERATED (static) |
| [bit_string](bit_string.md) | a BIT STRING of bytes, every bit or the first ones (static) |
| [octet_string](octet_string.md) | an OCTET STRING (static) |
| [null](null.md) | a NULL (static) |
| [object_identifier](object_identifier.md) | an OBJECT IDENTIFIER (static) |
| [utf8_string](utf8_string.md) | a UTF8String (static) |
| [printable_string](printable_string.md) | a PrintableString (static) |
| [ia5_string](ia5_string.md) | an IA5String (static) |
| [numeric_string](numeric_string.md) | a NumericString (static) |
| [visible_string](visible_string.md) | a VisibleString (static) |
| [bmp_string](bmp_string.md) | a BMPString (static) |
| [utc_time](utc_time.md) | a UTCTime (static) |
| [generalized_time](generalized_time.md) | a GeneralizedTime (static) |
| [sequence](sequence.md) | a SEQUENCE of elements (static) |
| [set](set.md) | a SET of elements in DER's order (static) |
| [explicit_tag](explicit_tag.md) | `[n] EXPLICIT`: an element around another (static) |
| [implicit_tag](implicit_tag.md) | `[n] IMPLICIT`: another element's content under a tag (static) |
| [raw](raw.md) | any tag over a content (static) |

#### Observers

| Function | Description |
|---|---|
| [operator bool](operator_bool.md) | whether it is an element |
| [cls](cls.md) | the class of its tag |
| [tag](tag.md) | the number of its tag |
| [constructed](constructed.md) | whether its content is elements |
| [is](is.md) | whether it is of a universal type |
| [is_context](is_context.md) | whether its tag is `[n]` |
| [content](content.md) | the content octets |
| [bytes](bytes.md) | the whole element, its DER |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | the element inside at an index |
| [size](size.md) | the elements inside |
| [empty](empty.md) | whether there is none inside |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | the first element inside |
| [end](end.md) | past the last element inside |

#### Values

| Function | Description |
|---|---|
| [as_bool](as_bool.md) | a BOOLEAN's value |
| [as_int](as_int.md) | an INTEGER or ENUMERATED within `int64_t` |
| [as_big_integer](as_big_integer.md) | an INTEGER of any size |
| [as_bytes](as_bytes.md) | an OCTET STRING's bytes |
| [as_bits](as_bits.md) | a BIT STRING's bits |
| [as_oid](as_oid.md) | an OBJECT IDENTIFIER |
| [as_string](as_string.md) | a string of any type, as UTF-8 |
| [as_time](as_time.md) | a UTCTime or GeneralizedTime |

#### Conversions

| Function | Description |
|---|---|
| [to_string](to_string.md) | an indented dump, an element a line |
| [hash](hash.md) | a hash of its bytes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two elements are the same bytes |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a public key of P-256 as X.509 writes it (RFC 5480)
    vector<byte> point(65, byte(4));
    encoding::asn1 key = encoding::asn1::sequence({
        encoding::asn1::sequence({encoding::asn1::object_identifier(encoding::asn1::oid("1.2.840.10045.2.1")),
                                  encoding::asn1::object_identifier(encoding::asn1::oid("1.2.840.10045.3.1.7"))}),
        encoding::asn1::bit_string(point)});
    println("{} bytes", key.bytes().size());

    auto read = encoding::asn1::parse(key.bytes());
    if (!read) {
        println(read.error().message());
        return 1;
    }
    encoding::asn1 spki = *read;
    println("curve {}", *spki[0][1].as_oid());
    println("{} bits of key", spki[1].as_bits()->length);
    print(spki.to_string());
}
```

Output:

```text
91 bytes
curve 1.2.840.10045.3.1.7
520 bits of key
SEQUENCE
  SEQUENCE
    OBJECT IDENTIFIER 1.2.840.10045.2.1
    OBJECT IDENTIFIER 1.2.840.10045.3.1.7
  BIT STRING (520 bits) 0404040404040404040404040404040404040404040404040404040404040404...
```

## See also

- [asn1::oid](../asn1-oid/README.md): an OBJECT IDENTIFIER
- [pem](../pem/README.md): the text a DER element travels in
- [json](../json/README.md): the same shape for JSON
- [big_integer](../../math/big_integer/README.md): an INTEGER of any size
- [sgcl::encoding](../README.md)
