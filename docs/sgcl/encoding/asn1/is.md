[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::is

```cpp
bool is(type t) const noexcept;
```

Whether the element is of the universal [type](../asn1-type.md) `t`: its class universal and its tag's
number the type's. An implicitly tagged element is of its tag, not of the type it holds. `false` for
[asn1()](asn1.md).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the type |

## Return value

`true` when the element is of the type.

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
    encoding::asn1 cert = encoding::asn1::sequence({
        encoding::asn1::explicit_tag(0, encoding::asn1::integer(2)),
        encoding::asn1::integer(4096),
        encoding::asn1::implicit_tag(1, encoding::asn1::bit_string(vector<byte>{byte(0x80)}, 1)),
        encoding::asn1::utf8_string("example.com")});

    println(cert.is(encoding::asn1::type::sequence));
    println(cert[1].is(encoding::asn1::type::integer));
    println(cert[2].is(encoding::asn1::type::bit_string));
}
```

Output:

```text
true
true
false
```

## See also

- [is_context](is_context.md): a context-specific tag
- [asn1::type](../asn1-type.md)
- [sgcl::encoding::asn1](README.md)
