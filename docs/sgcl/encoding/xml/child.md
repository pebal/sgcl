[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::child

```cpp
xml child(const string& name) const noexcept;
```

The first element of this name inside the element, or `xml()` when there is none, or the node is not an element.
`xml()` answers every question with nothing — no children, no attributes, an empty text — so that
`doc.child("a").child("b").text()` needs no check at each step. The name is matched as written (`dc:title`) or by
namespace and local name (`{http://purl.org/dc/elements/1.1/}title`); only the children are looked at.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element, as written or `{namespace}local` |

## Return value

The first element of that name, or `xml()`.

## Complexity

Linear in the number of children.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto doc = encoding::xml::parse(R"(<config>
  <server><host>example.com</host><port>8080</port></server>
</config>)").value();
    println(doc.child("server").child("host").text());
    println("[{}]", doc.child("client").child("host").text());
    println(doc.child("server").child("port").exists());
}
```

Output:

```text
example.com
[]
true
```

## See also

- [children](children.md): every node inside, or every element of a name
- [exists](exists.md): whether a node was found
- [sgcl::encoding::xml](README.md)
