[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::value

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class value;
}
```

One value handed to a [stencil](../stencil/README.md): text, a whole number, a real one, a truth, nothing, a list, or a mapping
of names to more of the same. C++20 has no reflection, so a template cannot walk the fields of a struct of the
program's and does not pretend to: the program builds a `value`, and the braces are arranged so that the call reads
as data — `txt::object{{"name", "Ada"}, {"scores", txt::list{91.5, 88.0}}}`. Where Go's `text/template` walks any
value by reflection, through `map[string]any` and an `interface{}` at every step, this is a variant of the seven
shapes ([value_kind](../value_kind.md)).

A `value` is thirty-two bytes — a [variant](../../core/variant/README.md) over nothing, a truth, a `long long`, a `double`, a
[string](../../core/string/README.md) and two pointers — and nothing at all is allocated for a number or a truth; text costs
what a string of the library costs, a pointer to characters that are shared and never copied. The two shapes that
hold other values, made by [list](../list/README.md) and [object](../object/README.md), reach them through a tracked pointer, so a copy
of a value shares what it holds, and a value may be put inside itself. A mapping keeps the order it was written in,
not the order of a hash ([ordered_map](../../core/ordered_map/README.md)): a page written twice running has to be the same
page.

## Rules

- A value holds a `tracked_ptr` (in its two compound shapes), so it lives where one may: on a stack or inside a
  managed object, a container of the library included ([the rules of core](../../core/README.md#the-rules), 1).
- One trap of the language comes with the braces and is worth saying out loud: `value v{"one"}` is a **list of one
  piece of text**, where `value v("one")` is the text. The initializer list wins in copy-list-initialization, as it
  does everywhere else in C++.
- A `char` and a `char32_t` are not a number and not text here: a character goes in as text, `"A"`.
- [format](../format.md) writes a whole value — `txt::format("{}", data)` — a list in brackets and a mapping in braces,
  as a template's field does. A [list](../list/README.md) or an [object](../object/README.md) is handed to it as a `value`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](value.md) | makes a value of nothing, a truth, a number, a text or a list |
| [kind](kind.md) | what the value holds |
| [is_none](is_none.md) | whether it holds nothing |
| [truthy](truthy.md) | what an `if`, a `with` and `default` ask |
| [size](size.md) | the elements of a list or a mapping |
| [find](find.md) | a mapping's value of a name |
| [at](at.md) | a list's element at an index |
| [text](text.md) | the text the value holds |
| [write](write.md) | writes the value into a sink, as a field of a template |
| [to_string](to_string.md) | the value as text, as `format` writes it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{
        {"name", "Ada"},
        {"born", 1815},
        {"tags", {"maths", "engines"}},  // a list inside a mapping needs no type named
        {"note", nullptr},
    };
    println("{}", data);
    println("{} {}", *data.find("name")->text(), data.find("tags")->size());
    return 0;
}
```

Output:

```text
{"name": "Ada", "born": 1815, "tags": ["maths", "engines"], "note": ""}
Ada 2
```

## See also

- [list](../list/README.md), [object](../object/README.md): the two shapes written as data
- [value_kind](../value_kind.md): what a value holds
- [stencil](../stencil/README.md): what renders one
