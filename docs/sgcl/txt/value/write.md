[sgcl](../../README.md) › [txt](../README.md) › [value](../value.md)

# sgcl::txt::value::write

```cpp
void write(format_sink& out, const format_spec& spec, std::string_view nested = {}) const noexcept;
```

Writes what the value holds into `out` the way [format](../format.md) writes it, with the
[specification](../format.md#the-specification) `spec` and, for a list or a mapping, the specification of the
elements `nested`: it is what a field of a [stencil](../stencil.md) does, and what `txt::format("{}", v)` calls.
Every alternative goes to `format`'s own writers; nothing is a second implementation. It is total:

- A specification the alternative will not take — `{:d}` over a name — is written with as much of it as the
  alternative takes: first a nested specification over a value that holds nothing is dropped, then the type letter,
  then the precision, `#` and `0`. The fill, the alignment and the width are kept, so the column the template asked for
  stays, with the value in it.
- `{:c}` of a whole number is the **code point** it is, written as the bytes it takes; a number that is no code point
  (negative, past U+10FFFF, a surrogate) drops the letter.
- Nothing is written as nothing, the field still filled; inside a list or a mapping, whose elements take the debug
  form, it is `""`.
- A value put inside itself is `...` where it would be written inside itself, so a ring of any breadth is cut
  where it closes. A value deeper than sixty-four levels is `...` there too.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the sink |
| `spec` | the specification of the field |
| `nested` | the specification of the elements, after a second colon; a view over nothing when there is none |

## Return value

None.

## Complexity

Linear in the length of what is written.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[64];
    txt::format_sink out(room, sizeof room);
    txt::value("ada").write(out, txt::format_spec{.align = '>', .width = 6, .type = 'd'});
    out.put('|');
    txt::value(233).write(out, txt::format_spec{.type = 'c'});
    out.put('|');
    txt::value(txt::list{1, 2}).write(out, txt::format_spec{.type = 'n'}, ">3");
    println("{}", std::string_view(room, out.size()));

    txt::object self;
    self.set("self", self);
    string deep = txt::value(self).to_string();
    println("{} characters, {}", deep.size(), deep.view().substr(deep.view().find("...") - 9, 16));
    return 0;
}
```

Output:

```text
   ada|é|  1,   2
15 characters, "self": "..."}
```

## See also

- [to_string](to_string.md): the same with no specification, as a string
- [format_sink](../format_sink.md), [format_spec](../format_spec.md)
- [sgcl::txt::value](../value.md)
