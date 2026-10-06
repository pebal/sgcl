[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::explicit_tag

```cpp
static asn1 explicit_tag(uint32_t number, const asn1& inner, tag_class c = tag_class::context_specific);
```

`[number] EXPLICIT`: a constructed element of the tag holding `inner` whole, its own tag and length
included — X.509's version `[0]` and extensions `[3]`. Read back, its value is its first element, `e[0]`. Of an
[asn1()](asn1.md) it is `asn1()`, so an OPTIONAL component stays a condition.

## Parameters

| Parameter | Description |
|---|---|
| `number` | the tag's number, below 2^28 |
| `inner` | the element inside |
| `c` | the [class](../asn1-tag_class.md) of the tag, context-specific unless another is given |

## Return value

The element.

## Complexity

Linear in the bytes of `inner`.

## Exceptions

`invalid_argument` for a number of 2^28 or more.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1 version = encoding::asn1::explicit_tag(0, encoding::asn1::integer(2));
    println(encoding::hex::encode(version.bytes()));
    println("v{}", *version[0].as_int() + 1);
    print(encoding::asn1::explicit_tag(5, encoding::asn1::null(), encoding::asn1::tag_class::application).to_string());
}
```

Output:

```text
a003020102
v3
[APPLICATION 5]
  NULL
```

## See also

- [implicit_tag](implicit_tag.md): the content under the tag
- [is_context](is_context.md)
- [sgcl::encoding::asn1](README.md)
