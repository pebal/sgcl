[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::xml

```cpp
xml() noexcept = default;                       // (1)
explicit xml(const string& name);               // (2)
xml(const string& name, const string& text);    // (3)
```

Constructs a node.

1. No node: `xml()`, of the kind `none`, what [child](child.md) gives when there is no such child. Nothing is
   allocated.
2. An empty element, `<name/>`. The name is a qualified name of Namespaces in XML (`book`, `dc:title`), not of
   the prefix `xmlns`, which no element has, or `invalid_argument` is thrown.
3. The element `<name>text</name>`: an element holding one text node; an empty `text` makes no child.

An element made here is in no namespace, but for the prefix `xml`, which stands for
`http://www.w3.org/XML/1998/namespace`, until it declares one: an `xmlns` attribute [set](set.md) on it (`xmlns`
for an element without a prefix, `xmlns:p` for `p:a`) puts it in that namespace, as [parse](parse.md) puts the
element it reads, and [erase](erase.md) of the declaration takes it out. A node does not know the elements around
it: a child made apart and added with [push_back](push_back.md) keeps the namespace it was made with, never its
parent's default. A prefix is not checked against a declaration either: `xml("p:a").to_string()` is `<p:a/>`, which
`parse` refuses while nothing declares `p`.

A copy of a node is a copy of the handle, one word; the tree is shared and never changes.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element, with its prefix |
| `text` | the text the element holds |

## Complexity

- (1) Constant.
- (2–3) Linear in the length of `name`, which is checked; the strings are shared, not copied.

## Exceptions

- (1) None.
- (2–3) `invalid_argument` when `name` is not a qualified name or is of the prefix `xmlns`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml none;
    encoding::xml empty("br");
    encoding::xml title("dc:title", "Lalka");
    println("{} {} {}", none.exists(), empty.to_string(), title.to_string());
    println("{} [{}]", title.local_name(), title.namespace_uri());
    println("{}", encoding::xml("xml:lang").namespace_uri());
    try {
        encoding::xml bad("1st");
    } catch (const invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
false <br/> <dc:title>Lalka</dc:title>
title []
http://www.w3.org/XML/1998/namespace
sgcl::encoding::xml: '1st' is not a qualified name
```

## See also

- [text_node](text_node.md), [comment](comment.md), [instruction](instruction.md): the other kinds of node
- [builder](../xml-builder.md): an element made a child at a time
- [parse](parse.md): the tree of a document
- [sgcl::encoding::xml](../xml.md)
