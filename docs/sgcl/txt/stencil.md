[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::stencil

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class stencil;
}
```

A text written from a shape the program did not write: a page, a letter, a report, whose form lives in a file and
is changed by whoever owns the words rather than by whoever owns the program. The shape is text with **actions** in
it — `{{ name }}`, `{{ if }}`, `{{ range }}` — and the values that go in are handed over by the caller as a
[value](value.md). A template is **read once and written many times** — that is the whole of what it is for — so
parsing and rendering are separate calls with a compiled form in between: [parse](stencil/parse.md) reads the source
into steps, and [render](stencil/render.md) walks them over the data, never looking at a character of the source but
to copy a run of it out.

It is Go's `text/template` in its syntax, with three departures, each on purpose: a bare name is a field of the
current element (Go reads it as a function call), a name the data does not carry writes nothing (Go writes
`<no value>`), and a field takes the specification of [format](format.md) after a colon where Go calls `printf`.
There is no escaping that knows where a value lands, which Go has in `html/template`. C++20 has no reflection, so a
template cannot walk the fields of a struct of the program's, as Go's walks them by reflection: the data is a
[value](value.md), built with braces so that the call reads as data.

It is held to Go's own `text/template` over the subset both can be asked:
[tools/stencil_oracle.go](../../../tools/stencil_oracle.go) puts fifty-three sources to it over one shape of data
and writes its answers out as the vectors the test uses. Where the two differ the case is left out rather than bent
until it passes, and the Go file says which and why — the bare name, a missing name, a walk over a mapping, the full
case mappings, and the spelling of a quote in `escape_html`; those five are checked by hand instead. The half no
oracle can reach is held to `format.h` directly, and both halves are fuzzed under the address and
undefined-behaviour sanitizers: twenty thousand sources nobody meant, every one either refused with a place and a
reason or rendered without a crash; two hundred thousand specifications after the colon, each rendered and then
asked again through [render_to](stencil/render_to.md), which must report the same size; and twenty thousand random
value trees over thirteen templates, `render` against `render_to` into a buffer with a guard behind it.

## Rules

- A template holds its source and its steps in [strings](../core/string.md) and [vectors](../core/vector.md), so it
  lives where those may: on a stack or inside a managed object ([the rules of core](../core/README.md#the-rules), 1).
  A render is `const` and changes nothing in it.
- It keeps the source rather than copying out of it: a string of the library is shared and immutable, so holding it
  costs a pointer, and every run of literal text in the page is a pair of offsets into it, written out whole.
- It keeps the functions its pipelines call, copied from the table at [parse](stencil/parse.md); the table need not
  outlive it.

### The syntax

| Action | Meaning |
|---|---|
| `{{ name }}` | a field of the current element |
| `{{ .name }}` | the same, written the way Go writes it |
| `{{ a.b.c }}` | a path down through mappings |
| `{{ . }}` | the current element itself |
| `{{ $ }}`, `{{ $.a }}` | the whole of the data, from any depth: the only way out of a `range` |
| `{{ "text" }}`, `{{ 'text' }}`, `{{ 42 }}`, `{{ -1.5 }}` | a literal; `\n`, `\t`, `\r`, `\\` and the quotes are its escapes |
| `{{ name:spec }}` | written with that [specification of format](format.md#the-specification) |
| `{{ name \| upper }}` | piped through a function, and through as many as one likes; `{{ n \| default 0 }}` with arguments |
| `{{ if x }}…{{ else if y }}…{{ else }}…{{ end }}` | the first arm whose value is true |
| `{{ range xs }}…{{ else }}…{{ end }}` | `.` is each element; the `else` arm is taken when there is nothing to walk |
| `{{ with x }}…{{ else }}…{{ end }}` | `.` becomes `x` where `x` is true |
| `{{/* … */}}` | a comment, and nothing inside it is read |
| `{{- x }}`, `{{ x -}}` | the white space before or after the action is taken away |

A name is letters, digits and `_`: a mapping whose keys are sentences is reached through the data, not through a
path. A **bare name is a field of the current element**, which is the one place this departs from Go's syntax: there
a name with no dot is a function call, here it is the shorthand that Jinja and Handlebars have. `{{ name }}` and
`{{ .name }}` mean the same thing.

**Truth** is the rule a reader of Go or of Python expects: nothing, `false`, a number that is nought, text with no
characters, and a list or a mapping with no elements are all false; everything else is true
([truthy](value/truthy.md)). A `range` over a number or a text takes the empty road.

A walk over a list makes `.` each element in turn, pointing at it and copying none of them. A walk over a mapping, in
the order it was written, makes `.` a row of `key` and `value`: `{{ range m }}{{ .key }}={{ .value }}{{ end }}`.

**A name the data does not carry writes nothing** — not `<no value>`, which is what Go writes — and still fills its
field, so `[{{ missing:>5 }}]` is five spaces in brackets and a column stays a column.

There is no escape for a literal `{{`: it is written as a literal value, `{{ "{{" }}`.

### A field is a field of format

A value may carry the **specification of [format](format.md)** after it, and that specification is read by
`format.h`'s own reader and written by `format.h`'s own writers: `{{ total:>8.2f }}` pads and rounds exactly as
`txt::format("{:>8.2f}", total)` does — the same grammar, the same refusals, the same writers. Nothing about
converting a number, padding a field, stopping a precision on a code point or escaping a debug form is written a
second time, and the [test](../../../tests/txt/stencil.cpp) asks both sides the same eleven questions and compares,
so the two cannot drift apart unnoticed. So a field of text is **measured in the columns it takes**, not in its
bytes, as everywhere in the module: `żółć` is eight bytes and four columns, so `{{ s:>10 }}` pads it by six. A
[formatter](formatter.md) of `value` carries it the rest of the way, so a list and a mapping come out in the shapes
of C++23, a list in brackets and a mapping in braces, their text elements in quotes.

Two things are not the same, both because a template knows no types where it is read:

- **The second colon.** A pattern knows the type of every value, so it can tell a colon that opens a specification
  for the elements from a colon that is merely a character to pad with: `txt::format("{::>4}", 7)` is `:::7`, a
  number holding nothing for a nested specification to be about. A template always reads the second colon as opening
  one, and where the value turns out to hold nothing the nested part is dropped and the rest of the field kept:
  `{{ n::>4 }}` over 7 writes `7`. A fill of colons is the whole of what is lost; any other fill character writes it,
  `{{ n:c>4 }}`.
- **A code point.** `{:c}` of `format` narrows to a `char` and throws over a number no `char` holds, the value being
  what is wrong and the caller being there to catch it. A template has no such caller — the letter comes from a file
  and the number from the data — so `{{ n:c }}` is the **code point** `n`, written as the bytes it takes, and a number
  that is no code point (negative, past U+10FFFF, a surrogate) drops the letter and keeps the column. That also keeps
  every page valid UTF-8, which narrowing would not: `{:c}` of 233 as a single byte is not a character at all.

The **type** a field will meet is the one thing that cannot be settled where the source is read. `{:d}` over a name
is that case: refusing the page would punish the reader for the template author's slip and writing nothing would
lose the value, so the type letter is dropped and the fill, the alignment and the width are kept —
`[{{ s:>6d }}]` over `ada` is `[   ada]`.

### The pipeline

`{{ x | f a b }}` hands what came down the pipe and the arguments written after the name — literals and paths both,
at most eight — to the function `f` of the table the template was parsed with, and what it answers goes on down.
The table of [stencil_functions](stencil_functions.md) has six always there — `upper`, `lower`, `title`, `trim`,
`escape_html` and `default` — and the program's own join them and may replace them. A template calling a name the
table does not know **fails to parse**, rather than writing nothing where a word was wanted.

**A function of the pipeline must be pure of side effects.** A render that does not fit the room it keeps on the stack
writes one step again — one field, into room twice the size, rather than the page — so the pipeline of that field
runs a second time, path and all. Which field that is depends on where the page happens to cross a kilobyte and each
doubling after it, so a function that counts, logs or reads a clock is right on most pages and wrong on some, and the
some are not the ones anybody tests ([stencil_function](stencil_function.md)). Nothing else in a render depends on it.

### The boundary: escaping is the caller's

**There is no escaping here that knows where a value lands.** Go has that in `html/template` — it reads the HTML
around a field and escapes for an attribute, a URL or a script accordingly. That is a parser for a second language
and a promise this module is not in a position to keep, so it is not made. What is here is `escape_html` as a
function of the pipeline, asked for by name: whoever writes the template decides where it is needed, and **the safety
of the HTML is the caller's**. In this module a boundary is written down rather than discovered.

### What may fail

A source that does not parse is a thing that really happens — a file somebody edited — so it is an answer and not an
exception, the register [format](format.md) uses for a pattern read where the program runs: [parse](stencil/parse.md)
gives back an `expected`, whose error, a [stencil_error](stencil_error.md), says where and why. A source the program
itself writes, a literal in the code, is not data that may be wrong: it is constructed,
`txt::stencil t("Hello, {{ name }}!")`, and a slip in it throws `bad_expected_access<stencil_error>` with the same
error — the mistake of the program, found the first time the line runs.

Everything that can be settled where the source is read is settled there: the shape of every specification (a width
past 65535 is refused like any other that does not fit), that every function named is one the table knows, that
every block is closed, that no jump goes nowhere.

A render is total over the values: the field and the data meeting for the first time cannot take a page down.
`{{ n:c }}` over a number no character holds is the number with the letter dropped, and a value **put inside
itself** — `object o; o.set("self", o)` shares one mapping, since a value reaches what it holds through a tracked
pointer — is written as far as the ring closes, and where a value would be written inside itself it is `...`: the
collector is happy with a cycle, and the writer cuts it where it closes whatever its breadth, a mapping that holds
itself under three names costing one level and not 3^64 of them. A value deeper than sixty-four levels is cut there
the same way. What a render may throw is only what a function of the pipeline throws.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](stencil/stencil.md) | an empty template, or the template a literal of the program spells |
| [parse](stencil/parse.md) | reads a source into a template, or says where and why it is not one |
| [parses](stencil/parses.md) | whether a source is a template, with nothing kept |
| [render](stencil/render.md) | writes the page of the data |
| [render_to](stencil/render_to.md) | writes the page into memory the caller lends |
| [steps](stencil/steps.md) | how many steps the source came to |
| [source](stencil/source.md) | the source |

## Example

A value written with a specification of `format`, and a field of text measured in columns:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::stencil t("{{ total:>8.2f }} | {{ name:?}} | {{ s:>10 }}|");
    println("{}", t.render(txt::object{{"total", 3.14159}, {"name", "Ada"}, {"s", "żółć"}}));
    return 0;
}
```

Output:

```text
    3.14 | "Ada" |       żółć|
```

A list in the shapes of C++23, the second colon belonging to its elements:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"xs", txt::list{1, 2, 3}}};
    for (auto source : {"{{ xs }}", "{{ xs::>4 }}", "{{ xs:n }}"}) {
        println("{}", txt::stencil(source).render(data));
    }
    return 0;
}
```

Output:

```text
[1, 2, 3]
[   1,    2,    3]
1, 2, 3
```

Over a value that holds nothing the nested part is dropped, where a pattern pads with colons:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"n", 7}};
    println("[{}] [{}]", txt::stencil("{{ n::>4 }}").render(data),
            txt::stencil("{{ n:c>4 }}").render(data));
    println("[{}]", txt::format("{::>4}", 7));
    return 0;
}
```

Output:

```text
[7] [ccc7]
[:::7]
```

`{{ n:c }}` is the code point `n`, and a number that is no code point keeps its column:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::stencil letter("{{ n:c }}");
    for (int n : {65, 233, 12345}) {
        print("{} ", letter.render(txt::object{{"n", n}}));
    }
    println("{}", txt::stencil("[{{ n:>6c }}]").render(txt::object{{"n", -1}}));
    return 0;
}
```

Output:

```text
A é 〹 [    -1]
```

The blocks, the paths and a name the data does not carry:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"title", "Scores"}, {"rows", txt::list{
        txt::object{{"name", "Ada"}, {"score", 91.5}},
        txt::object{{"name", "Alan"}},
    }}};
    txt::stencil t("{{ title }}:\n"
                   "{{- range rows }}\n"
                   "  {{ name:<5 }}{{ if score }}{{ score:>5.1f }}{{ else }}    -{{ end }}"
                   " ({{ $.title }})\n"
                   "{{- end }}");
    println("{}", t.render(data));
    return 0;
}
```

Output:

```text
Scores:
  Ada   91.5 (Scores)
  Alan     - (Scores)
```

## See also

- [value](value.md), [list](list.md), [object](object.md): the data
- [stencil_functions](stencil_functions.md): the functions a pipeline may call
- [stencil_error](stencil_error.md): where and why a source is not a template
- [format](format.md): whose specification a field carries
- [to_upper_full](to_upper_full.md), [to_lower_full](to_lower_full.md), [to_title](to_title.md): the full
  mappings `upper`, `lower` and `title` use
- [ordered_map](../core/ordered_map.md): the mapping a value keeps
