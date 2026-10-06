[sgcl](../../README.md) › [txt](../README.md) › [message_format](README.md)

# sgcl::txt::message_format::format

```cpp
string format(const value& args) const noexcept;
```

Returns the message with the arguments: an [object](../stencil/README.md) by name or a list by position. An argument
is written by its type: with none, a number in the locale's way and anything else as its text; a number by its
style; a date or a time (the value milliseconds since 1970, in UTC, or RFC 3339 text with its offset) by
[sgcl/time](../../time/README.md), which registers itself when it is included (without it, the value's text); a
plural or a selectordinal by the case of its `=value`, else of the CLDR category of the number minus the offset, `#`
in it the number minus the offset; a select by the case of its text, else `other`. An argument nobody gave is
written `{name}`, as ICU writes it; a value of another type than the argument's, as its text.

## Parameters

| Parameter | Description |
|---|---|
| `args` | the arguments: a txt::object or a txt::list |

## Return value

The text.

## Complexity

Linear in the length of the text written.

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::message_format
        m("{g, select, female {She} other {He}} has {n, plural, =0 {no files} "
          "one {one file} other {# files}} of {size, number, ::compact-short} bytes.",
          txt::locale("en"));
    println("{}", m.format(txt::object{{"g", "female"}, {"n", 0}, {"size", 1250000}}));
    println("{}", m.format(txt::object{{"g", "male"}, {"n", 12}, {"size", 4300}}));
}
```

Output:

```text
She has no files of 1.2M bytes.
He has 12 files of 4.3K bytes.
```

## See also

- [parse](parse.md)
- [format_message](../format_message.md)
- [sgcl::txt::message_format](README.md)
