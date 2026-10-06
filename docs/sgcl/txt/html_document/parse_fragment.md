[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::parse_fragment

```cpp
static html_document parse_fragment(const string& text, const string& context = string("body"),
                                    const html_options& o = {});
```

Reads a fragment as the standard's fragment algorithm (13.4) reads it — what innerHTML of an element of the
context's name reads: a `<td>` is a cell in the context of a `tr` and nothing in a `body`, text is text in a
`textarea` or a `script`. The document's [root](root.md) is an `html` element holding the fragment's nodes.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the fragment |
| `context` | the local name of the element it is read into, in any case; `body` by default |
| `o` | the options |

## Return value

The document of the fragment.

## Complexity

As [parse](parse.md).

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {

    println("{}", txt::html_document::parse_fragment("<td>a<td>b", "tr").to_string());
    println("{}", txt::html_document::parse_fragment("<td>a<td>b").to_string());
    println("{}", txt::html_document::parse_fragment("<b>x</b>", "textarea").to_string());
}
```

Output:

```text
<td>a</td><td>b</td>
ab
&lt;b&gt;x&lt;/b&gt;
```

## See also

- [parse](parse.md)
- [sanitize_html](../sanitize_html.md)
- [sgcl::txt::html_document](README.md)
