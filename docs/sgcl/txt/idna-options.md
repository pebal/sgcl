[sgcl](../README.md) › [txt](README.md) › [idna](idna.md)

# sgcl::txt::idna::options

```cpp
#include "sgcl/txt/idna.h"   // or "sgcl/txt.h"

namespace sgcl::txt::idna {
    struct options {
        bool transitional = false;
        bool use_std3_ascii_rules = false;
        bool check_hyphens = true;
        bool check_bidi = true;
        bool check_joiners = true;
        bool verify_dns_length = true;
        bool ignore_invalid_punycode = false;

        static constexpr options standard() noexcept;
        static constexpr options whatwg() noexcept;
    };
}
```

The flags of [UTS #46](https://www.unicode.org/reports/tr46/), taken by [to_ascii](idna/to_ascii.md),
[to_unicode](idna/to_unicode.md), [ascii_form](idna/ascii_form.md) and [unicode_form](idna/unicode_form.md). They are
a value and not a set of tags, because a program does not choose them where it is compiled: a URL parser is told what
profile to follow, and the same call serves a strict lookup and a lenient one.

The defaults, [standard()](idna-options/standard.md), are the strict reading: everything checked, the name held to
what the DNS will carry. A browser's URL parser wants [whatwg()](idna-options/whatwg.md) instead, which is a looser
profile on purpose: the WHATWG URL Standard leaves the hyphens and the lengths alone so that names already in the wild
keep working, while the bidirectional and joiner rules are still checked.

## Rules

- An aggregate of `bool`s, trivially copyable, passed by value; a field is set by name,
  `options{.transitional = true}`, or on a copy of a profile.

## Member objects

| Member | Description |
|---|---|
| `transitional` | transitional processing, which UTS #46 deprecates: the four deviation characters mapped instead of kept, `faß.de` becoming `fass.de` rather than `xn--fa-hia.de` and a Greek final sigma an ordinary one; it silently sends two different names to the same host, and nothing new should ask for it ([idna](idna.md)); `false` by default |
| `use_std3_ascii_rules` | only the letters, the digits and the hyphen let through in ASCII (RFC 1123); `false` by default, as the browsers have it, since an underscore in a host name is common enough that refusing it surprises people. With it off UTS #46 lets a great deal of ASCII through that IDNA2008 itself would not, an underscore and, more surprisingly, `a♥b.com`, since the mapping table marks such characters valid for UTS #46 and only notes (`NV8`) that IDNA2008 would refuse them; refusing more than that is the caller's business |
| `check_hyphens` | refuses `--` in the third and fourth places of a label and a hyphen at either end; `true` by default |
| `check_bidi` | holds a name with anything right to left in it to the six conditions of RFC 5893; `true` by default |
| `check_joiners` | holds the zero width joiners to the ContextJ rules of RFC 5892; `true` by default |
| `verify_dns_length` | refuses an empty label, unless it is the last one and something came before it (`example.com.`); `to_ascii` also holds every label to 1 to 63 bytes, the trailing empty one included, and the name to 253, the root label and its dot not counted. Without it an empty label is no error, as UTS #46 has it since Unicode 15.1 and as the WHATWG URL Standard needs (`x..ß` is the host `x..xn--zca`); `true` by default |
| `ignore_invalid_punycode` | leaves a label that says `xn--` and is not punycode as it stands rather than refusing it, for a parser that must not lose a name it cannot read; `false` by default |

## Member functions

| Function | Description |
|---|---|
| [standard](idna-options/standard.md) | the strict profile, the defaults |
| [whatwg](idna-options/whatwg.md) | the profile of the WHATWG URL Standard |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::idna::options strict;
    txt::idna::options std3{.use_std3_ascii_rules = true};
    for (auto name : {"a_b.com", "a♥b.com"}) {
        auto plain = txt::idna::to_ascii(name, strict);
        auto narrow = txt::idna::to_ascii(name, std3);
        println("{}: {} | {}", name, plain ? *plain : plain.error().message(),
                narrow ? *narrow : narrow.error().message());
    }
}
```

Output:

```text
a_b.com: a_b.com | an ASCII character outside letters, digits and the hyphen
a♥b.com: xn--ab-t0x.com | xn--ab-t0x.com
```

## See also

- [to_ascii](idna/to_ascii.md), [to_unicode](idna/to_unicode.md): what takes the options
- [sgcl::txt::idna](idna.md)
