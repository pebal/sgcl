[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::operator== (sgcl::encoding::xml)

```cpp
friend bool operator==(const xml& a, const xml& b) noexcept;
```

Checks whether two nodes are the same tree: the same kind, the same names as written and the same namespaces, the
same values, the same children in the same order; the attributes in any order, as XML has them. Two `xml()` are
equal. A node and its copy, which share the tree, are equal at once, and so is every subtree two versions share.
`a != b` is `!(a == b)`, which C++20 writes from `==`.

A prefix is part of the name: `<p:a xmlns:p="u"/>` and `<q:a xmlns:q="u"/>` are not equal. The tree is walked with
a loop of its own, not recursion.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the nodes to compare |

## Return value

`true` when the nodes are equal, `false` otherwise.

## Complexity

Linear in the size of the smaller tree, at most; constant for a node and its copy.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto a = encoding::xml::parse("<a x='1' y='2'><b/></a>").value();
    auto b = encoding::xml::parse("<a y='2' x='1'>\n  <b/>\n</a>").value();
    println(a == b);
    println(a == a.set("x", "3"));
    auto p = encoding::xml::parse("<p:a xmlns:p='u'/>").value();
    auto q = encoding::xml::parse("<q:a xmlns:q='u'/>").value();
    println(p == q);
    println(encoding::xml() != encoding::xml("a"));
}
```

Output:

```text
true
false
false
true
```

## See also

- [to_string](to_string.md): what `parse` reads back as the same tree
- [sgcl::encoding::xml](README.md)
