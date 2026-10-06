[sgcl](../../README.md) › [txt](../README.md) › [markdown_document](README.md)

# sgcl::txt::markdown_document::to_html

```cpp
string to_html() const;
```

Returns the document's HTML, written with the options it was parsed with: what
[markdown_to_html](../markdown_to_html.md) returns for its text.

## Parameters

None.

## Return value

The HTML.

## Complexity

Linear in the size of the tree.

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
    print("{}", doc.to_html());
}
```

Output:

```text
<h1>Notes</h1>
<p>Some <em>text</em> and <code>code</code>, a <a href="/a" title="A">link</a>.</p>
<ol start="3">
<li>three</li>
<li><input type="checkbox" checked="" disabled="" /> four</li>
</ol>
<pre><code class="language-cpp">int x;
</code></pre>
<table>
<thead>
<tr>
<th align="left">a</th>
<th align="right">b</th>
</tr>
</thead>
<tbody>
<tr>
<td align="left">1</td>
<td align="right">2</td>
</tr>
</tbody>
</table>
```

## See also

- [markdown_node::to_html](../markdown_node/to_html.md)
- [sgcl::txt::markdown_document](README.md)
