[sgcl](../../README.md) › [txt](../README.md) › [html_stencil](README.md)

# sgcl::txt::html_stencil::render

```cpp
string render(const value& data) const;
```

Returns the page of `data`: [stencil](../stencil/README.md)'s, each field escaped for its place as Go's
html/template escapes it. A URL whose scheme is not http, https or mailto is written `#ZgotmplZ`, a CSS value or an
attribute's name that could leave its place `ZgotmplZ`; in a script a value is written as JSON (an object in the
order its fields were set). A field piped last through `safe_html`, `safe_url`, `safe_attr`, `safe_js` or `safe_css`
is trusted in that context: HTML in text (in an attribute, its tags stripped), a URL as it is (only normalized), an
attribute's name and value, a script's expression, a CSS value.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the values |

## Return value

The page.

## Complexity

Linear in the length of the page.

## Exceptions

What a function of the pipeline throws; running out of memory ends the program.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::html_stencil page(
        R"html(<a href="{{ url }}" title="{{ title }}" onclick="go({{ id }})">{{ name }}</a>
<script>var user = {{ user }};</script>)html");
    println("{}", page.render(txt::object{{"url", "javascript:alert(1)"},
                                          {"title", "\" onmouseover=\"x"},
                                          {"id", "7"},
                                          {"name", "<b>Ada</b>"},
                                          {"user", txt::object{{"id", 7}, {"name", "Ada"}}}}));
}
```

Output:

```text
<a href="#ZgotmplZ" title="&#34; onmouseover=&#34;x" onclick="go(&#34;7&#34;)">&lt;b&gt;Ada&lt;/b&gt;</a>
<script>var user = {"id":7,"name":"Ada"};</script>
```

## See also

- [render_to](render_to.md)
- [stencil::render](../stencil/render.md)
- [sgcl::txt::html_stencil](README.md)
