[sgcl](../../README.md) › [txt](../README.md) › [idna](../idna.md)

# sgcl::txt::idna::to_ascii

```cpp
expected<string, failure> to_ascii(const string& name, options o = {});
```

Returns the name as the DNS carries it, by section 4.2 of [UTS #46](https://www.unicode.org/reports/tr46/): mapped
(the case folded, the compatibility forms and the ideographic full stops written plainly, the default ignorable code
points dropped), normalized to NFC, every label held to the checks [options](../idna-options.md) ask for, and every
label with something above ASCII in it encoded as [punycode](../punycode/encode.md) and prefixed with `xn--`. A name
that is wrong in any of those ways is not a name: nothing comes back but the rule it broke, and there is no text to
use by mistake. [ascii_form](ascii_form.md) gives the text as well.

With `verify_dns_length`, the default, every label is held to 1 to 63 bytes, the last one included, so the trailing
dot of a fully qualified name, `example.com.`, is refused here as `IdnaTestV2.txt` has it, and the whole name to 253
bytes, the root label and its dot not counted. A name that is already what the DNS carries, lower case letters,
digits, hyphens and stops and no `xn--` label, is checked over its bytes and comes back as the same object.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, UTF-8, its labels separated by `.` or an ideographic full stop |
| `o` | what is checked; the strict reading by default, `options::whatwg()` for a URL parser |

## Return value

The name in ASCII, or the [failure](../idna-failure.md): the first rule the name broke and the label that broke it.

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
    for (auto name :
         {"bücher.example.de", "WWW.Example.COM", "faß.de", "ab--cd.com", "example.com."}) {
        auto host = txt::idna::to_ascii(name);
        println("{}: {}", name, host ? *host : host.error().message());
    }
    println("{}", txt::idna::to_ascii("x..ß", txt::idna::options::whatwg()).value());
}
```

Output:

```text
bücher.example.de: xn--bcher-kva.example.de
WWW.Example.COM: www.example.com
faß.de: xn--fa-hia.de
ab--cd.com: a hyphen in the third and fourth place, or at an end
example.com.: a label with nothing in it
x..xn--zca
```

## See also

- [to_unicode](to_unicode.md): the other way
- [ascii_form](ascii_form.md): the text even when it is wrong
- [sgcl::txt::idna](../idna.md)
