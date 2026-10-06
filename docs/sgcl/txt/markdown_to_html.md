[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::markdown_to_html

```cpp
#include "sgcl/txt/markdown.h"   // or "sgcl/txt.h"

string markdown_to_html(const string& text, const markdown_options& o = {});
```

Returns the HTML of a Markdown text: CommonMark 0.31.2 — every block and inline rule of the specification, link
reference definitions, entities, the delimiter rules of emphasis — and GitHub's extensions: tables, strikethrough,
extended autolinks (`www.`, `http://`, `https://`, e-mail addresses), task list items. The HTML is cmark's, byte for
byte in its form: `<br />`, `<hr />`, a code block's language as `class="language-…"`, a tight list's paragraphs
without `<p>`. Safe by default: raw HTML is left out (a comment, `<!-- raw HTML omitted -->`, in its place) and the
destination of a link or an image to `javascript:`, `vbscript:`, `file:` or `data:` (but images of PNG, GIF, JPEG
and WebP) is emptied. Never fails: every text is a document.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the Markdown, UTF-8 (a byte that is not UTF-8 passes through; NUL becomes U+FFFD) |
| `o` | the [markdown_options](markdown_options.md): the extensions, raw HTML, line breaks, URLs |

## Return value

The HTML.

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
    print("{}", txt::markdown_to_html(
                    "# Title\n\nSome *emphasis* and **strong** text.\n\n- one\n- [x] two\n"));
    print("{}", txt::markdown_to_html("<b>raw</b> [x](javascript:alert(1))\n"));
    print("{}", txt::markdown_to_html("<b>raw</b>\n", {.raw_html = true}));
}
```

Output:

```text
<h1>Title</h1>
<p>Some <em>emphasis</em> and <strong>strong</strong> text.</p>
<ul>
<li>one</li>
<li><input type="checkbox" checked="" disabled="" /> two</li>
</ul>
<p><!-- raw HTML omitted -->raw<!-- raw HTML omitted --> <a href="">x</a></p>
<p><b>raw</b></p>
```

## See also

- [markdown_document](markdown_document/README.md)
- [markdown_options](markdown_options.md)
- [sanitize_html](sanitize_html.md)
- [sgcl::txt](README.md)
