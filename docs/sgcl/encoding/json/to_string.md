[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::to_string

```cpp
string to_string(const style& s = compact) const;
```

The text of the value, Go's `json.Marshal` of an `any`; with [pretty](../json.md#member-objects), `MarshalIndent`
with two spaces. Compact by default, with no space at all. The members of an object are written in their order, a
number with the shortest digits that read back as the same double, as JavaScript and Go write it: fixed from
1e-6 to 1e21 and with an exponent outside (`1e+21`, `1e-7`), no `.0` on an integer, −0 as `-0`. A value always
has a text: a string's invalid UTF-8 is written as U+FFFD, and a json holds no NaN.

With an indent, each element and member is on a line of its own, a space after the colon, and an empty array or
object stays `[]` or `{}`. [style](../json-style.md)`::escape_html` writes `<`, `>` and `&` as `<`, `>`
and `&`; its `sort_keys` is for the maps of a program's type ([stringify](stringify.md)) and changes nothing
here.

## Parameters

| Parameter | Description |
|---|---|
| `s` | how the value is written; `compact` by default |

## Return value

The text, UTF-8.

## Complexity

Linear in the size of the value.

## Exceptions

`length_error` when the text would pass the 4 GiB a [string](../../core/string.md) holds.

## Notes

The writing walks the value with a stack of its own, so a value built by hand deeper than any stack of calls is
written as well. An indented text grows with the square of the depth; it is measured where its block grows,
and the writing stops as soon as it would pass the limit of a string, so a value deep enough for terabytes of
indentation fails at once. A thread keeps its block and its stack from one writing to the next: nothing is allocated but
the string.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(
        R"({"b": [1e21, 1e-7, 2.0, -0, 0.1], "a": {}, "html": "<a&b>"})");
    println(doc.to_string());
    println(doc.to_string(encoding::json::pretty));

    encoding::json::style safe;
    safe.escape_html = true;
    println(doc["html"].to_string(safe));
}
```

Output:

```text
{"b":[1e+21,1e-7,2,-0,0.1],"a":{},"html":"<a&b>"}
{
  "b": [
    1e+21,
    1e-7,
    2,
    -0,
    0.1
  ],
  "a": {},
  "html": "<a&b>"
}
"\u003ca\u0026b\u003e"
```

## See also

- [stringify](stringify.md): the text of a program's value
- [save](save.md): the text into a file
- [parse](parse.md): the way back
- [sgcl::encoding::json](../json.md)
