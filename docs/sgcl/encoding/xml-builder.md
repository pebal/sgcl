[sgcl](../README.md) › [encoding](README.md) › [xml](xml.md)

# sgcl::encoding::xml::builder

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        class builder;
    };
}
```

`sgcl::encoding::xml::builder` makes an element an attribute and a child at a time, and hands it out as an
[xml](xml.md) node when it is done. The node's own [push_back](xml/push_back.md) and [set](xml/set.md) copy the
whole node each time, so an element of n children made by them costs n²; the builder gathers the attributes and the
children in lists of its own and makes the node once, in [build](xml-builder/build.md).

## Rules

- The name of the element and of each attribute is checked when it is given: one that is not a qualified name is
  `invalid_argument`, as `push_back(xml())` is.
- The namespaces of the element and of its attributes are found when the element is built, from what the element
  itself tells: its `xmlns` declarations and its own prefix. An element whose declarations do not name its prefix
  is in no namespace, but for the prefix `xml`, as the [constructor](xml/xml.md) of a node makes it.
- `build()` leaves the builder empty, with the same name, ready for the next element.
- A builder holds tracked pointers (its lists): it lives where a `tracked_ptr` may. A copy has lists of its own.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xml-builder/xml-builder.md) | a builder of an element of a name |

#### Modifiers

| Function | Description |
|---|---|
| [set](xml-builder/set.md) | sets an attribute |
| [push_back](xml-builder/push_back.md) | adds a node as the last child |
| [build](xml-builder/build.md) | the element; the builder is left empty |

## Complexity

- `set`: linear in the number of attributes set.
- `push_back`: amortized constant.
- `build`: linear in the number of attributes and children.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::builder list("list");
    for (int i : range(3)) {
        list.push_back(encoding::xml("item", to_string(i * i)));
    }
    println(list.build().to_string());
}
```

Output:

```text
<list><item>0</item><item>1</item><item>4</item></list>
```

## See also

- [xml::push_back](xml/push_back.md), [xml::set](xml/set.md): one change, a new node
- [writer](xml-writer.md): XML of any length onto a stream, without a tree
- [sgcl::encoding::xml](xml.md)
