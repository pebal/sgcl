[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::text

```cpp
string text() const;
```

Returns the text of the node and every descendant, in order: texts, code spans, code blocks and raw HTML as they
are, line breaks as `\n` — what a reader sees, without the markup.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the subtree.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::markdown_document::parse(
        "# Notes\n\nSome *text* and `code`, a [link](/a \"A\").\n\n"
        "3. three\n4. [x] four\n\n~~~cpp\nint x;\n~~~\n\n| a | b |\n|:-|-:|\n| 1 | 2 |\n");
    println("{}", doc.root()[1].text());
    println("{}", doc.root()[0].text());
}
```

Output:

```text
Some text and code, a link.
Notes
```

## See also

- [literal](literal.md)
- [sgcl::txt::markdown_node](README.md)
