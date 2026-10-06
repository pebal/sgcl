[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::parse

```cpp
static html_document parse(const string& text, const html_options& o = {});
```

Reads a document as a browser reads it: the WHATWG HTML parsing algorithm over UTF-8 text (a byte that is not UTF-8
read as U+FFFD, CR LF and CR as LF), the tokenizer and every insertion mode of the tree construction — the adoption
agency for misnested formatting, foster parenting for text in tables, templates, SVG and MathML. It never fails:
every error is recovered from, as the standard says, and reported only when the options ask.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the document |
| `o` | the [html_options](../html_options.md): the scripting flag, whether the errors are kept |

## Return value

The document.

## Complexity

Linear in the length of the text, for the documents of practice (the standard's algorithm walks its stacks of open
and formatting elements, which a page keeps short).

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {

    auto mended = txt::html_document::parse("<p>a<b>b<i>c</p>d");
    println("{}", mended.body().inner_html());
}
```

Output:

```text
<p>a<b>b<i>c</i></b></p><b><i>d</i></b>
```

## See also

- [parse_fragment](parse_fragment.md)
- [to_string](to_string.md)
- [html_options](../html_options.md)
- [sgcl::txt::html_document](README.md)
