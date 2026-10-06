[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::markdown_options

```cpp
#include "sgcl/txt/markdown.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct markdown_options {
        bool tables = true;
        bool strikethrough = true;
        bool autolinks = true;
        bool task_lists = true;
        bool raw_html = false;
        bool tag_filter = true;
        bool hard_breaks = false;
        bool safe_urls = true;
    };
}
```

`sgcl::txt::markdown_options` is how [markdown_to_html](markdown_to_html.md) and
[markdown_document](markdown_document/README.md) read and write Markdown; all off but `tag_filter` and `safe_urls`,
it is CommonMark alone.

## Member objects

| Field | Description |
|---|---|
| `tables` | GitHub's tables: a header row, a delimiter row of `-` and `:`, rows of cells between pipes; `true` by default |
| `strikethrough` | `~text~` and `~~text~~` as `<del>`; `true` by default |
| `autolinks` | links without `<` and `>`: `www.` and `http://`, `https://`, `ftp://` addresses, e-mail addresses; `true` by default |
| `task_lists` | `[ ]` and `[x]` at the start of an item as a checkbox; `true` by default |
| `raw_html` | the text's HTML written as it is; `false` by default, which writes `<!-- raw HTML omitted -->` in its place |
| `tag_filter` | with `raw_html`, GitHub's tag filter: the `<` of `title`, `textarea`, `style`, `xmp`, `iframe`, `noembed`, `noframes`, `script` and `plaintext` written as `&lt;`; `true` by default |
| `hard_breaks` | every line break a `<br />`; `false` by default |
| `safe_urls` | links and images to `javascript:`, `vbscript:`, `file:` and `data:` (but images) lose their destination; `true` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "| a |\n| - |\n\n~~gone~~ www.example.com\nnext line\n";
    print("{}",
          txt::markdown_to_html(text,
                                {.tables = false, .strikethrough = false, .autolinks = false}));
    print("{}", txt::markdown_to_html("a\nb\n", {.hard_breaks = true}));
}
```

Output:

```text
<p>| a |
| - |</p>
<p>~~gone~~ www.example.com
next line</p>
<p>a<br />
b</p>
```

## See also

- [markdown_to_html](markdown_to_html.md)
- [markdown_document](markdown_document/README.md)
- [sgcl::txt](README.md)
