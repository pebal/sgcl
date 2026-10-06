[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::to_html

```cpp
string to_html(const markdown_options& o = {}) const;
```

Returns the HTML of the node and its descendants, as [markdown_to_html](../markdown_to_html.md) writes a document: a
heading, a list, a paragraph alone.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the [markdown_options](../markdown_options.md) of the writing: raw HTML, line breaks, URLs |

## Return value

The HTML.

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
    print("{}", doc.root()[2].to_html());
    print("{}", doc.root()[1].to_html());
}
```

Output:

```text
<ol start="3">
<li>three</li>
<li><input type="checkbox" checked="" disabled="" /> four</li>
</ol>
<p>Some <em>text</em> and <code>code</code>, a <a href="/a" title="A">link</a>.</p>
```

## See also

- [markdown_document::to_html](../markdown_document/to_html.md)
- [sgcl::txt::markdown_node](README.md)
