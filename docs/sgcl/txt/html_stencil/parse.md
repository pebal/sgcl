[sgcl](../../README.md) › [txt](../README.md) › [html_stencil](README.md)

# sgcl::txt::html_stencil::parse

```cpp
static expected<html_stencil, stencil_error> parse(const string& source) noexcept;         // (1)
static expected<html_stencil, stencil_error> parse(const string& source,
                                                   const stencil_functions& functions);    // (2)
```

Reads a template from outside the program: [stencil](../stencil/README.md)'s syntax, then its text as HTML, which
gives each field the escaping of its place — text, an attribute's name or value, a URL (its start, its path, its
query), a srcset, JavaScript (an expression, a string, a template literal, a regular expression) or CSS (a value, a
string, a url()). HTML, JavaScript and CSS comments in the text are dropped from the page, and a `<` of the text
that begins no tag is written `&lt;`.

1. With the six functions every template has and the five `safe_` ones.
2. With a table of the caller's, which the `safe_` functions are added to.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the template |
| `functions` | the functions it may call |

## Return value

The template, or a [stencil_error](../stencil_error/README.md): what [stencil::parse](../stencil/parse.md) refuses,
and where the HTML gives a field no safe place, as Go's html/template refuses it — branches of an `if`, a `with` or
a `range` that leave the page in different contexts, a source that ends inside a tag, an attribute, a script, a
style or a comment, a quote or `<` in an attribute's name, a quote, `<`, `=` or a backtick in an unquoted value, a
field inside the character class of a regular expression, a slash or a URL part that the branches before it leave
unknown.

## Complexity

Linear in the length of the source.

## Exceptions

None that the program can cause; running out of memory ends it (2: the callables of `functions` are copied with
theirs).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"<p title=\"{{ x }}\">", "{{ if b }}<a href=\"{{ end }}", "<script>{{ x }}"}) {
        auto t = txt::html_stencil::parse(s);
        if (t) {
            println("{}", t->render(txt::object{{"x", "\"&"}}));
        } else {
            println("line {}: {}", t.error().line(), t.error().message());
        }
    }
}
```

Output:

```text
<p title="&#34;&amp;">
line 1: branches end in different HTML contexts
line 1: the template ends inside a tag, an attribute, a script, a style or a comment
```

## See also

- [(constructor)](html_stencil.md)
- [render](render.md)
- [stencil_error](../stencil_error/README.md)
- [sgcl::txt::html_stencil](README.md)
