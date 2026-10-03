[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [builder](../xml-builder.md)

# sgcl::encoding::xml::builder::push_back

```cpp
builder& push_back(const xml& child);
```

Adds `child` as the last child of the element: an element, a text, a comment or an instruction. The node is
shared, not copied. `xml()` is no node, and is `invalid_argument`.

## Parameters

| Parameter | Description |
|---|---|
| `child` | the node to add |

## Return value

`*this`.

## Complexity

Amortized constant.

## Exceptions

`invalid_argument` when `child` is `xml()`; the builder is unchanged.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::builder p("p");
    p.push_back(encoding::xml::text_node("Lines: "));
    for (int i : range(1, 4)) {
        p.push_back(encoding::xml("line", to_string(i)));
    }
    p.push_back(encoding::xml::comment(" end "));
    println(p.build().to_string());
}
```

Output:

```text
<p>Lines: <line>1</line><line>2</line><line>3</line><!-- end --></p>
```

## See also

- [set](set.md): an attribute
- [xml::push_back](../xml/push_back.md): one child added, a new node
- [sgcl::encoding::xml::builder](../xml-builder.md)
