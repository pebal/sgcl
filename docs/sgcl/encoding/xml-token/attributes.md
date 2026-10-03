[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [token](README.md)

# sgcl::encoding::xml::token::attributes

```cpp
slice<const xml::attr> attributes() const noexcept;
```

The attributes of a start, in the order of the tag ([xml::attr](../xml-attr.md): the name as written,
the value with its references replaced and its white space normalized, the namespace), the `xmlns` declarations
among them. Empty for the other tokens. The slice holds the array of the token.

## Parameters

None.

## Return value

The attributes, or an empty slice.

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
    encoding::xml::reader r("<img src='a.png' alt='A &amp; B'/>");
    auto start = r.next().value();
    for (auto& a : start.attributes()) {
        println("{}={}", a.name, a.value);
    }
}
```

Output:

```text
src=a.png
alt=A & B
```

## See also

- [attribute](attribute.md): the value of one attribute
- [xml::attr](../xml-attr.md)
- [sgcl::encoding::xml::token](README.md)
