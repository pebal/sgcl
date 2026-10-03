[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::push_back

```cpp
xml push_back(const xml& child) const;
```

A new version of this element with `child` added as its last child: an element, a text, a comment or an
instruction. The node itself never changes. The new node copies the arrays of attributes and children, and shares
the children, `child` among them: a subtree is put in many trees without a copy.

Each call copies the array of children, so an element of n children made by n calls costs n²: an element of many
children is made with [builder](../xml-builder.md).

## Parameters

| Parameter | Description |
|---|---|
| `child` | the node to add |

## Return value

The element with the node added.

## Complexity

Linear in the number of attributes and children of the element.

## Exceptions

`invalid_argument` when the node is not an element, or `child` is `xml()`. The node is unchanged.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml empty("ul");
    encoding::xml one = empty.push_back(encoding::xml("li", "first"));
    encoding::xml two = one.push_back(encoding::xml("li", "second"));
    println("{} {} {}", empty.to_string(), one.children().size(), two.to_string());
    try {
        two.push_back(two.child("ol"));
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
<ul/> 1 <ul><li>first</li><li>second</li></ul>
sgcl::encoding::xml::push_back: xml() is no node
```

## See also

- [builder](../xml-builder.md): an element made a child at a time
- [children](children.md): the nodes inside an element
- [sgcl::encoding::xml](../xml.md)
