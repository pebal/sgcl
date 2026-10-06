[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::html_stencil

```cpp
#include "sgcl/txt/html_stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class html_stencil;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::html_stencil` is a template whose page is HTML: [stencil](../stencil/README.md)'s syntax and values,
every field written escaped for the place in the page where it lands, as Go's html/template writes it. The
template's own text is read as HTML once, when the source is parsed, and each `{{ }}` is given the escaping of its
context. Go's html/template is the oracle of its tests.

## Rules

- The template is trusted, the data is not: a field cannot close an attribute or a string, open a tag
  or a script, or carry a scheme other than http, https or mailto into a URL. Only the template marks a value
  trusted, with `safe_html`, `safe_url`, `safe_attr`, `safe_js` or `safe_css` last in its pipeline (Go's typed
  strings); the data alone never can.
- The syntax, the values, the functions and the specifications are stencil's; the bare names of stencil and Go's
  dotted ones are read alike.
- Where it does not follow Go: an object in a script is written in the order its fields were set, where Go's JSON
  sorts the keys; a list or an object in an HTML context is written as stencil writes it, where Go writes its fmt
  form; entities in an attribute's text are decoded for the analysis by their numeric forms and amp, lt, gt, quot,
  apos and nbsp, where Go knows every name.
- A value of read-only state: any number of threads render one template.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](html_stencil.md) | constructs the empty template, or the one a literal spells |
| [parse](parse.md) | reads a template from outside the program (static) |
| [parses](parses.md) | checks whether a source is one (static) |

#### Rendering

| Function | Description |
|---|---|
| [render](render.md) | the page of the values |
| [render_to](render_to.md) | the page into memory the caller lends |

#### Observers

| Function | Description |
|---|---|
| [source](source.md) | the text it was read from |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::html_stencil page(R"(<ul>{{ range items }}
<li><a href="/search?q={{ . }}" title="{{ . }}">{{ . }}</a></li>{{ end }}
</ul><script>var n = {{ count }};</script>)");
    println("{}",
            page.render(txt::object{{"items", txt::list{"a&b", "<i>", "x y"}}, {"count", 3}}));
}
```

Output:

```text
<ul>
<li><a href="/search?q=a%26b" title="a&amp;b">a&amp;b</a></li>
<li><a href="/search?q=%3ci%3e" title="&lt;i&gt;">&lt;i&gt;</a></li>
<li><a href="/search?q=x%20y" title="x y">x y</a></li>
</ul><script>var n =  3 ;</script>
```

## See also

- [stencil](../stencil/README.md)
- [stencil_error](../stencil_error/README.md)
- [Benchmarks](../benchmarks.md)
- [sgcl::txt](../README.md)
