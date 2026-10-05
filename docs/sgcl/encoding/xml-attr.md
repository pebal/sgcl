[sgcl](../README.md) › [encoding](README.md) › [xml](xml/README.md)

# sgcl::encoding::xml::attr

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        struct attr {
            string name;
            string value;
            string namespace_uri;

            friend bool operator==(const attr&, const attr&) = default;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::xml::attr` is an attribute as the document writes it: the name with its prefix, the value
with its references replaced and its white space normalized (XML 1.0, 3.3.3), and the namespace the prefix stands
for. [attributes](xml/attributes.md) of a node and [attributes](xml-token/attributes.md) of a token give them in
the order of the tag.

The method [attribute](xml/attribute.md) gives the value of one attribute by its name. Go's `xml.Attr` is the
same, with its name split into the namespace and the local part.

## Member objects

| Member | Description |
|---|---|
| `name` | the name as written: `xlink:href`, `id`; empty by default |
| `value` | the value, references replaced, white space normalized; empty by default |
| `namespace_uri` | the namespace of the prefix; empty for a name without one; `http://www.w3.org/2000/xmlns/` for `xmlns` and `xmlns:p`; empty by default |

## Non-member functions

| Function | Description |
|---|---|
| `operator==` | checks whether the three members are equal (defaulted) |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto a = encoding::xml::parse("<a xmlns:x='urn:x' x:k=' 1&#65;2\n3 '/>").value();
    for (auto& at : a.attributes()) {
        println("{} [{}] [{}]", at.name, at.value, at.namespace_uri);
    }
    encoding::xml::attr k{"x:k", " 1A2 3 ", "urn:x"};
    println(a.attributes()[1] == k);
}
```

Output:

```text
xmlns:x [urn:x] [http://www.w3.org/2000/xmlns/]
x:k [ 1A2 3 ] [urn:x]
true
```

## See also

- [attributes](xml/attributes.md), [attribute](xml/attribute.md): the attributes of a node
- [token::attributes](xml-token/attributes.md): the attributes of a start tag
- [sgcl::encoding::xml](xml/README.md)
