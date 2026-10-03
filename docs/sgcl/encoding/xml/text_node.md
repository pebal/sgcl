[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::text_node

```cpp
static xml text_node(const string& text) noexcept;
```

A text node holding `text`, for an element's content made a node at a time ([push_back](push_back.md),
[builder](../xml-builder.md)). Any text is taken: what must be escaped is escaped when the node is written, and a
character XML cannot hold is written as U+FFFD.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text |

## Return value

The text node, of the kind `text`.

## Complexity

Constant: the node shares the string.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml p = encoding::xml("p")
                          .push_back(encoding::xml::text_node("1 < 2 & "))
                          .push_back(encoding::xml("b", "bold"))
                          .push_back(encoding::xml::text_node("!"));
    println(p.to_string());
    println("{} {}", p.children()[0].is_text(), p.text());
}
```

Output:

```text
<p>1 &lt; 2 &amp; <b>bold</b>!</p>
true 1 < 2 & bold!
```

## See also

- [comment](comment.md), [instruction](instruction.md): the other nodes of an element's content
- [text](text.md): the text of a node
- [sgcl::encoding::xml](../xml.md)
