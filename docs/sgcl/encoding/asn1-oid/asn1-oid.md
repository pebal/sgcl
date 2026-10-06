[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::oid

```cpp
constexpr oid() noexcept;                                   // (1)
template<size_t N> consteval oid(const char (&text)[N]);    // (2)
explicit oid(const string& text);                           // (3)
oid(std::initializer_list<uint64_t> arcs);                  // (4)
```

1. No arcs: `false`, the identifier of nothing, which [object_identifier](../asn1/object_identifier.md) refuses.
2. Of a dotted literal, in a constant expression: a literal that is not an identifier is an error of the compiler.
   Explicit, as every constructor of a text: `asn1::oid("2.5.4.3")`.
3. Of a dotted text the program writes: [parse](parse.md)'s value, or `parse`'s error thrown.
4. Of the arcs, each within 64 bits.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the identifier dotted |
| `arcs` | the arcs, the first two among them |

## Complexity

- (1–2) Constant.
- (3–4) Linear in the length of the identifier.

## Exceptions

- (3) [bad_expected_access](../../core/bad_expected_access/README.md)`<encoding::error>` with `parse`'s error.
- (4) `invalid_argument` for fewer than two arcs, a first past 2, a second past 39 under 0 and 1, or more than 63
  bytes of DER.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1::oid none;
    constexpr encoding::asn1::oid cn("2.5.4.3");
    encoding::asn1::oid sha256({2, 16, 840, 1, 101, 3, 4, 2, 1});
    println("{} {} {}", bool(none), cn, sha256);
    try {
        encoding::asn1::oid({1, 40});
    } catch (const invalid_argument& error) {
        println(error.what());
    }
}
```

Output:

```text
false 2.5.4.3 2.16.840.1.101.3.4.2.1
sgcl::encoding::asn1::oid: a first arc past 2 or a second past 39
```

## See also

- [parse](parse.md): a text from outside
- [sgcl::encoding::asn1::oid](README.md)
