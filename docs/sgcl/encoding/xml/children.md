[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::children

```cpp
slice<const xml> children() const noexcept;                    // (1)
generator<xml> children(const string& name) const noexcept;    // (2)
```

The nodes inside the element.

1. Every node inside, in order: elements, texts, and the comments and instructions the tree keeps
   ([options](../xml-options.md)). Empty for a node that is not an element. The slice holds the array of the
   node, so it stays valid as long as it is kept.
2. The elements of this name inside, in order, one at a time: a [generator](../../core/generator.md). The name is
   matched as written (`dc:title`) or by namespace and local name (`{http://purl.org/dc/elements/1.1/}title`).
   The node and the name are kept by the generator, so it may be called on a temporary.

Only the children are looked at, not their descendants.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the elements, as written or `{namespace}local` |

## Return value

1. The children, or an empty slice.
2. A generator of the elements of that name.

## Complexity

1. Constant.
2. Linear in the number of children, over the whole walk.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto list = encoding::xml::parse("<list><i>1</i>and<j/><i>2</i></list>").value();
    println(list.children().size());
    for (auto item : list.children("i")) {
        println(item.text());
    }
    for (auto b : encoding::xml::parse("<a><b/><b/></a>")->children("b")) {
        println(b.to_string());
    }
}
```

Output:

```text
4
1
2
<b/>
<b/>
```

## See also

- [child](child.md): the first element of a name
- [push_back](push_back.md): the element with one more child
- [sgcl::encoding::xml](../xml.md)
