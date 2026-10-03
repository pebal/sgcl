[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::namespace_uri

```cpp
string namespace_uri() const noexcept;
```

The namespace of an element's name: what its prefix, or the default namespace when it has none, stood for where
the element was read. An empty string for an element in no namespace, for a node that is not an element and for
`xml()`. An element made by the [constructor](xml.md) or a [builder](../xml-builder.md) is in the namespace its own
`xmlns` declaration gives its prefix, and else in none, but for the prefix `xml`.

A method that takes a name ([child](child.md), [children](children.md), [attribute](attribute.md)) matches the
namespace and the local name when the name is written `{namespace}local`.

## Parameters

None.

## Return value

The namespace, or an empty string.

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
    auto feed = encoding::xml::parse(R"(<feed xmlns="http://www.w3.org/2005/Atom">
  <title>News</title>
  <x:extra xmlns:x="urn:x" xmlns=""><plain/></x:extra>
</feed>)").value();
    println("[{}]", feed.namespace_uri());
    println("[{}]", feed.child("title").namespace_uri());
    auto extra = feed.child("{urn:x}extra");
    println("[{}] [{}]", extra.namespace_uri(), extra.child("plain").namespace_uri());
}
```

Output:

```text
[http://www.w3.org/2005/Atom]
[http://www.w3.org/2005/Atom]
[urn:x] []
```

## See also

- [name](name.md), [local_name](local_name.md)
- [sgcl::encoding::xml](../xml.md)
