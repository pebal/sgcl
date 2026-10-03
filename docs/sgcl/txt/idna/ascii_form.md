[sgcl](../../README.md) › [txt](../README.md) › [idna](README.md)

# sgcl::txt::idna::ascii_form

```cpp
outcome ascii_form(const string& name, options o = {});
```

Returns the name as the DNS carries it and what was wrong with it, by section 4.2 of
[UTS #46](https://www.unicode.org/reports/tr46/): what [to_ascii](to_ascii.md) does, with the text kept when the name
fails. UTS #46 converts as far as it can even when it fails, and that text is worth having: a browser shows the user
the name it would not look up, with the label the [failure](../idna-failure/README.md) names marked.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, UTF-8, its labels separated by `.` or an ideographic full stop |
| `o` | what is checked; the strict reading by default, `options::whatwg()` for a URL parser |

## Return value

The [outcome](../idna-outcome/README.md): the text as far as it was converted, and the failure, `error::none` when nothing
was wrong.

## Complexity

Linear in the length of the name.

## Exceptions

`length_error` when the result would pass the [max_size()](../../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string name = "bücher.-b\u00AD.com";
    auto made = txt::idna::ascii_form(name);
    println("{} {}", made.text, bool(made));
    auto why = made.reason;
    println("{}: label {}, bytes {} to {}", why.message(), why.label, why.at, why.at + why.size);
}
```

Output:

```text
xn--bcher-kva.-b.com false
a hyphen in the third and fourth place, or at an end: label 1, bytes 8 to 12
```

## See also

- [to_ascii](to_ascii.md): the name, or only the rule it broke
- [unicode_form](unicode_form.md): the other way
- [sgcl::txt::idna](README.md)
