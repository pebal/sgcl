[sgcl](../README.md) › [txt](README.md) › [idna](idna/README.md)

# sgcl::txt::idna::error

```cpp
#include "sgcl/txt/idna.h"   // or "sgcl/txt.h"

namespace sgcl::txt::idna {
    enum class error : uint8_t {
        none,
        disallowed,
        not_normalized,
        hyphen,
        label_prefix,
        label_separator,
        leading_combining,
        joiner,
        bidi,
        std3,
        punycode,
        empty_label,
        label_too_long,
        name_too_long,
    };
}
```

Which rule a name broke, the `rule` of a [failure](idna-failure/README.md). One name can break several at once and a failure
holds the first of them. The codes in the table are the steps of [UTS #46](https://www.unicode.org/reports/tr46/)
and the codes `IdnaTestV2.txt` writes, so that a failure can be read against the standard;
[message_of](idna/message_of.md) says each in words.

| Value | Description |
|---|---|
| `none` | no error |
| `disallowed` | V7: a code point no domain name may hold |
| `not_normalized` | V1: a decoded label that is not NFC |
| `hyphen` | V2, V3: `--` in the third and fourth place, or a hyphen at an end |
| `label_prefix` | V4: `xn--` where the hyphens are not being checked |
| `label_separator` | V5: a full stop inside a label |
| `leading_combining` | V6: a label beginning with a combining mark |
| `joiner` | C1, C2: a zero width joiner where the script does not call for one |
| `bidi` | B1–B6: the rule of RFC 5893 broken, in a name that runs right to left |
| `std3` | U1: an ASCII character that is not a letter, a digit or a hyphen, under `use_std3_ascii_rules` |
| `punycode` | P4, A3: an `xn--` label that is not punycode |
| `empty_label` | X4_2, A4_2: a label with nothing in it |
| `label_too_long` | A4_2: a label of more than 63 bytes |
| `name_too_long` | A4_1: a name of more than 253 bytes |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto name : {"ab--cd.com", "a..c", "aא.com", "a\u200Db.com"}) {
        auto host = txt::idna::to_ascii(name);
        using txt::idna::error;
        auto rule = host ? error::none : host.error().rule;
        println("{} {} {} {}", rule == error::hyphen, rule == error::empty_label,
                rule == error::bidi, rule == error::joiner);
    }
}
```

Output:

```text
true false false false
false true false false
false false true false
false false false true
```

## See also

- [failure](idna-failure/README.md): the rule and the label
- [message_of](idna/message_of.md): the rule in words
- [sgcl::txt::idna](idna/README.md)
