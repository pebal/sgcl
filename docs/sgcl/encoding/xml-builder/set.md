[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [builder](../xml-builder.md)

# sgcl::encoding::xml::builder::set

```cpp
builder& set(const string& name, const string& value);
```

Sets the attribute `name` to `value`: in the place of the attribute of the same name set before, matched as
written, or added at the end. Its namespace is found when the element is [built](build.md).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the attribute, with its prefix |
| `value` | its value, as it is: escaped when the element is written |

## Return value

`*this`.

## Complexity

Linear in the number of attributes set, amortized.

## Exceptions

`invalid_argument` when `name` is not a qualified name, or the attribute is a namespace declaration Namespaces in
XML forbids, as [xml::set](../xml/set.md) says; the builder is unchanged.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::builder b("svg:svg");
    b.set("xmlns:svg", "http://www.w3.org/2000/svg").set("width", "10").set("width", "20");
    b.set("svg:version", "1.1");
    encoding::xml svg = b.build();
    println(svg.to_string());
    for (auto& a : svg.attributes()) {
        println("{} [{}]", a.name, a.namespace_uri);
    }
}
```

Output:

```text
<svg:svg xmlns:svg="http://www.w3.org/2000/svg" width="20" svg:version="1.1"/>
xmlns:svg [http://www.w3.org/2000/xmlns/]
width []
svg:version [http://www.w3.org/2000/svg]
```

## See also

- [push_back](push_back.md): a child
- [xml::set](../xml/set.md): one attribute set, a new node
- [sgcl::encoding::xml::builder](../xml-builder.md)
