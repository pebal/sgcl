[sgcl](../../README.md) › [txt](../README.md) › [idna](README.md)

# sgcl::txt::idna::to_unicode

```cpp
expected<string, failure> to_unicode(const string& name, options o = {});
```

Returns the name as a reader would write it, by section 4.3 of [UTS #46](https://www.unicode.org/reports/tr46/):
mapped and normalized as [to_ascii](to_ascii.md) maps it, every label that was punycode read back into the script it
was written in, every label held to the checks [options](../idna-options/README.md) ask for. What a program shows somebody
of a name it was given in ASCII. A name that is wrong is not a name: nothing comes back but the rule it broke.
[unicode_form](unicode_form.md) gives the text as well.

The lengths are not checked here: they are the DNS's, and this is the name for a reader. With `verify_dns_length`,
an empty label is still refused unless it is the last one and something came before it, so `example.com.` passes.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, UTF-8, its labels separated by `.` or an ideographic full stop |
| `o` | what is checked; the strict reading by default, `options::whatwg()` for a URL parser |

## Return value

The name in Unicode, or the [failure](../idna-failure/README.md): the first rule the name broke and the label that broke it.

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
    for (auto name : {"xn--bcher-kva.example.de", "BÜCHER.de", "xn--fa-hia.de", "a..c"}) {
        auto shown = txt::idna::to_unicode(name);
        println("{}: {}", name, shown ? *shown : shown.error().message());
    }
}
```

Output:

```text
xn--bcher-kva.example.de: bücher.example.de
BÜCHER.de: bücher.de
xn--fa-hia.de: faß.de
a..c: a label with nothing in it
```

## See also

- [to_ascii](to_ascii.md): the other way
- [unicode_form](unicode_form.md): the text even when it is wrong
- [sgcl::txt::idna](README.md)
