[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::attribute

```cpp
optional<string> attribute(const string& name) const noexcept;                  // (1)
string attribute(const string& name, const string& fallback) const noexcept;    // (2)
```

The value of an attribute of the element, its references replaced and its white space normalized.

1. The value, or `nullopt` when the element has no such attribute, or the node is not an element.
2. The same with a value for when there is none: `e.attribute("lang", "en")`.

The name is matched as written, prefix included (`id`, `xlink:href`), or by namespace and local name when it is
written `{namespace}local` (`{http://www.w3.org/1999/xlink}href`); an attribute with no prefix is in no namespace,
`{}id`. The `xmlns` declarations are attributes too, in the namespace `http://www.w3.org/2000/xmlns/`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, as written or `{namespace}local` |
| `fallback` | the value when there is no such attribute |

## Return value

1. The value of the attribute, or `nullopt`.
2. The value of the attribute, or `fallback`.

## Complexity

Linear in the number of attributes of the element.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto use = encoding::xml::parse(
        R"(<use xmlns:xl="http://www.w3.org/1999/xlink" xl:href="#dot" id="u1" x="10"/>)").value();
    println(use.attribute("xl:href").value());
    println(use.attribute("{http://www.w3.org/1999/xlink}href").value());
    println(use.attribute("{}id").value());
    println(use.attribute("href").has_value());
    println(use.attribute("y", "0"));
}
```

Output:

```text
#dot
#dot
u1
false
0
```

## See also

- [attributes](attributes.md): every attribute, in order
- [set](set.md), [erase](erase.md): the element with an attribute changed
- [sgcl::encoding::xml](../xml.md)
