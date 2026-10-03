[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::attributes

```cpp
slice<const attr> attributes() const noexcept;
```

Every attribute of the element, in the order of the document ([attr](../xml-attr.md): the name as
written, the value, the namespace), the `xmlns` declarations among them. Empty for a node that is not an element.
The slice holds the array of the node, so it stays valid as long as it is kept.

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
    auto a = encoding::xml::parse("<a xmlns:p='urn:p' p:k='1' id='x'/>").value();
    for (auto& at : a.attributes()) {
        println("{} = {} [{}]", at.name, at.value, at.namespace_uri);
    }
    println(encoding::xml::text_node("t").attributes().size());
}
```

Output:

```text
xmlns:p = urn:p [http://www.w3.org/2000/xmlns/]
p:k = 1 [urn:p]
id = x []
0
```

## See also

- [attribute](attribute.md): the value of one attribute
- [xml::attr](../xml-attr.md)
- [sgcl::encoding::xml](README.md)
