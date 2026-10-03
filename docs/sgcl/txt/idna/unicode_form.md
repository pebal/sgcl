[sgcl](../../README.md) › [txt](../README.md) › [idna](../idna.md)

# sgcl::txt::idna::unicode_form

```cpp
outcome unicode_form(const string& name, options o = {});
```

Returns the name as a reader would write it and what was wrong with it, by section 4.3 of
[UTS #46](https://www.unicode.org/reports/tr46/): what [to_unicode](to_unicode.md) does, with the text kept when the
name fails, because that is the text there is to show somebody. A name that is already what the DNS carries comes
back as the same object.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, UTF-8, its labels separated by `.` or an ideographic full stop |
| `o` | what is checked; the strict reading by default, `options::whatwg()` for a URL parser |

## Return value

The [outcome](../idna-outcome.md): the text as far as it was converted, and the failure, `error::none` when nothing
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
    auto shown = txt::idna::unicode_form("xn--bcher-kva.-x-.de");
    println("{} {}", shown.text, bool(shown));
    println("{} in label {}", shown.reason.message(), shown.reason.label);
}
```

Output:

```text
bücher.-x-.de false
a hyphen in the third and fourth place, or at an end in label 1
```

## See also

- [to_unicode](to_unicode.md): the name, or only the rule it broke
- [ascii_form](ascii_form.md): the other way
- [sgcl::txt::idna](../idna.md)
