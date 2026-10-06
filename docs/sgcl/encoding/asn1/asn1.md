[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::asn1

```cpp
asn1() noexcept;
```

No element: `false` as a condition, what [operator\[\]](operator_at.md) gives past the end and what a
component left out is. [sequence](sequence.md) and [set](set.md) skip it, and [explicit_tag](explicit_tag.md) and
[implicit_tag](implicit_tag.md) of it give it back, so an OPTIONAL component is a condition in the list. Every
`as_*` of it is `nullopt`, its [bytes](bytes.md) are empty and its [size](size.md) is 0.

An element is made by the static functions that name its type ([integer](integer.md), [sequence](sequence.md)…)
or read by [parse](parse.md). The copy and the move are the implicit ones and copy the view: both elements see the
same bytes, which never change.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    bool signed_too = false;
    encoding::asn1 version = signed_too ? encoding::asn1::integer(1) : encoding::asn1();
    encoding::asn1 s = encoding::asn1::sequence({version, encoding::asn1::utf8_string("x")});
    println("{} element(s)", s.size());
    println("{}", bool(encoding::asn1()));
    println("{}", bool(s[5]));
}
```

Output:

```text
1 element(s)
false
false
```

## See also

- [sequence](sequence.md): an element made of others
- [parse](parse.md): an element read
- [sgcl::encoding::asn1](README.md)
