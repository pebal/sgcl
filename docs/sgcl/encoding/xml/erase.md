[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::erase

```cpp
xml erase(const string& name) const;
```

A new version of this element without the attribute `name`, matched as written. When the element has no such
attribute, the element itself is returned. Erasing a declaration, `xmlns` or `xmlns:p`, takes its namespace from
the element whose name has that prefix and from the attributes of the prefix `p`: they are in no namespace then, as
far as the element tells. The node itself never changes; the new node copies the arrays of
attributes and children, and shares the children.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the attribute, with its prefix |

## Return value

The element without the attribute.

## Complexity

Linear in the number of attributes and children of the element.

## Exceptions

`invalid_argument` when the node is not an element. The node is unchanged.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto input = encoding::xml::parse("<input name='q' disabled='disabled' size='20'/>").value();
    encoding::xml enabled = input.erase("disabled");
    println(input.to_string());
    println(enabled.to_string());
    println(enabled.erase("nothing") == enabled);
}
```

Output:

```text
<input name="q" disabled="disabled" size="20"/>
<input name="q" size="20"/>
true
```

## See also

- [set](set.md): the element with an attribute set
- [sgcl::encoding::xml](README.md)
