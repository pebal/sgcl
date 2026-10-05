[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::stencil_functions

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class stencil_functions;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

The functions a pipeline of a [stencil](../stencil/README.md) may call, by name: a table made with no arguments holds the six
that are always there, and [add](add.md) joins one of the program's to them or replaces one of the
same name. It is handed to [parse](../stencil/parse.md), which resolves every name written in the source against it, so a
template calling a function nobody wrote **fails to parse** rather than writing nothing where a word was wanted. Go's
`template.FuncMap` is the same idea, added with `Funcs` before the parse.

| Function | What it answers |
|---|---|
| `upper`, `lower`, `title` | the **full** case mappings ([to_upper_full](../to_upper_full.md), [to_lower_full](../to_lower_full.md), [to_title](../to_title.md)) — `straße` upper-cases to `STRASSE`, and a letter whose mapping depends on what stands around it is given what stands around it |
| `trim` | the text with the white space off both ends |
| `escape_html` | the text with `&`, `<`, `>`, `"` and `'` as the entities `&amp;`, `&lt;`, `&gt;`, `&quot;` and `&#39;` (not `&apos;`, which HTML 4 did not have) |
| `default` | its argument, where what came down the pipe is empty — false by the rule of [truthy](../value/truthy.md): nothing, nought and no characters all take it, as in Go |

Each of the first five works on the value as text ([to_string](../value/to_string.md)), so `{{ n | upper }}` of a number
is its digits.

## Rules

- A template keeps the functions it calls, copied at [parse](../stencil/parse.md), so the table need not outlive the
  templates parsed with it.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](stencil_functions.md) | a table of the six |
| [add](add.md) | joins a function, or replaces one of the same name |
| [find](find.md) | the function of a name |
| [builtin](builtin.md) | the table of the six, shared |

## Example

Escaping, asked for by name where the template author knows it is needed; a value that lands in a URL is not HTML
text, and nothing here knows that:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"comment", "<b>hi</b> & bye"}};
    println("{}", txt::stencil("<p>{{ comment | escape_html }}</p>").render(data));
    return 0;
}
```

Output:

```text
<p>&lt;b&gt;hi&lt;/b&gt; &amp; bye</p>
```

The other five:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"word", "straße"}, {"name", "  ADA lovelace "}, {"count", 0}};
    txt::stencil t("{{ word | upper }} {{ name | trim | lower | title }}! "
                   "{{ count | default \"none\" }}");
    println("{}", t.render(data));
    return 0;
}
```

Output:

```text
STRASSE Ada Lovelace! none
```

## See also

- [stencil_function](../stencil_function.md): what a function is
- [stencil](../stencil/README.md#the-pipeline): the pipeline
