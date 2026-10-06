[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::literal

```cpp
string literal() const noexcept;
```

Returns the text of a text, a code span, a code block or raw HTML, its escapes and entities read (but in raw HTML
and code).

## Parameters

None.

## Return value

The text; empty for other nodes.

## Complexity

Constant.

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
    println("[{}] [{}]", doc.root()[1][0].literal(), doc.root()[1][3].literal());
    print("{}", doc.root()[3].literal());
}
```

Output:

```text
[Some ] [code]
int x;
```

## See also

- [text](text.md)
- [info](info.md)
- [sgcl::txt::markdown_node](README.md)
