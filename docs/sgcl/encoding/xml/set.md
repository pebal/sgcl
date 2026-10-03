[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::set

```cpp
xml set(const string& name, const string& value) const;
```

A new version of this element with the attribute `name` set to `value`: in the place of the attribute of the same
name, matched as written, or added at the end. The node itself never changes. The new node copies the arrays of
attributes and children; the children themselves are shared.

The namespace of a new attribute is what the element itself tells: `http://www.w3.org/2000/xmlns/` for `xmlns` and
`xmlns:p`, the XML namespace for the prefix `xml`, the namespace of a prefix the element declares or its own name
carries, and none otherwise. A node does not know the elements around it. A declaration set — `xmlns`, or
`xmlns:p` — gives its namespace to the element when the element's name has that prefix (`xmlns`: no prefix), and to
the element's attributes of the prefix `p`, as [parse](parse.md) gives them: `xml("svg").set("xmlns", uri)` is the
element `<svg xmlns="…"/>` reads as.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the attribute, with its prefix |
| `value` | its value, as it is: escaped when the node is written |

## Return value

The element with the attribute set.

## Complexity

Linear in the number of attributes and children of the element.

## Exceptions

`invalid_argument` when the node is not an element, `name` is not a qualified name, or the attribute is a namespace
declaration Namespaces in XML forbids, which no reader would take: the prefix `xmlns` declared, `xml` bound to
another namespace or another prefix to its, anything bound to `http://www.w3.org/2000/xmlns/`, a prefix undeclared
(`xmlns:p=""`). The node is unchanged.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml img = encoding::xml("img").set("src", "a.png").set("alt", "A");
    encoding::xml bigger = img.set("width", "200").set("src", "b.png");
    println(img.to_string());
    println(bigger.to_string());
    encoding::xml svg = encoding::xml("svg").set("xmlns", "http://www.w3.org/2000/svg");
    println("{}", svg.namespace_uri());
    try {
        encoding::xml::text_node("t").set("a", "1");
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
<img src="a.png" alt="A"/>
<img src="b.png" alt="A" width="200"/>
http://www.w3.org/2000/svg
sgcl::encoding::xml::set: not an element
```

## See also

- [erase](erase.md): the element without an attribute
- [attribute](attribute.md): the value of an attribute
- [builder](../xml-builder.md): an element made an attribute and a child at a time
- [sgcl::encoding::xml](../xml.md)
