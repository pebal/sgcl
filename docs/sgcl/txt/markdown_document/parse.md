[sgcl](../../README.md) › [txt](../README.md) › [markdown_document](README.md)

# sgcl::txt::markdown_document::parse

```cpp
static markdown_document parse(const string& text, const markdown_options& o = {});
```

Reads a Markdown text into a tree of immutable [markdown_node](../markdown_node/README.md)s, as
[markdown_to_html](../markdown_to_html.md) reads it, and keeps the options for [to_html](to_html.md). Never fails.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the Markdown |
| `o` | the [markdown_options](../markdown_options.md) |

## Return value

The document.

## Complexity

Linear in the length of the text, for the documents of practice.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {

    auto root = txt::markdown_document::parse("Hello *world*.\n").root();
    println("{} {}", root.size(), root[0].size());
}
```

Output:

```text
1 3
```

## See also

- [root](root.md)
- [markdown_to_html](../markdown_to_html.md)
- [sgcl::txt::markdown_document](README.md)
