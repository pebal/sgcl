[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::nfc_t, sgcl::txt::nfd_t, sgcl::txt::nfkc_t, sgcl::txt::nfkd_t

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct nfc_t {};
    struct nfd_t {};
    struct nfkc_t {};
    struct nfkd_t {};

    inline constexpr nfc_t nfc {};
    inline constexpr nfd_t nfd {};
    inline constexpr nfkc_t nfkc {};
    inline constexpr nfkd_t nfkd {};
}
```

The tags of the four normalization forms of [UAX #15](https://www.unicode.org/reports/tr15/), passed to
[normalize](normalize.md) and [is_normalized](is_normalized.md): `normalize(s, txt::nfc)`.

`nfd` and `nfkd` are **decomposed**: every character taken apart into its base and its marks, the marks in the order
the standard fixes. `nfc` and `nfkc` are **composed**: decomposed first and then put back together, which is the
form the web, most protocols and most file systems want.

The `k` forms decompose by **compatibility** as well: `"ﬁ"` becomes `"fi"`, `"①"` becomes `"1"`, `"Ａ"` becomes
`"A"`, `"½"` becomes `"1⁄2"`. That keeps the meaning and loses the appearance, and it cannot be undone: they are
for comparing and indexing, not for storing.

## Rules

- The form is a **tag, not an enumeration**. `normalize(s, nfc)` reads at the call site as an enumerator would, the
  walk has no branch at run time, and a program that never asks for a `k` form does not carry the compatibility
  table, which is more than half the data of the normalization.
- Empty types, passed by value, written by their constants.

## Non-member functions

#### Constants

| Constant | Value | Description |
|---|---|---|
| `nfc` | `nfc_t{}` | canonical decomposition, then canonical composition, `inline constexpr nfc_t` |
| `nfd` | `nfd_t{}` | canonical decomposition, `inline constexpr nfd_t` |
| `nfkc` | `nfkc_t{}` | compatibility decomposition, then canonical composition, `inline constexpr nfkc_t` |
| `nfkd` | `nfkd_t{}` | compatibility decomposition, `inline constexpr nfkd_t` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include <type_traits>

using namespace sgcl;

int main() {
    string s = "ﬁancé";  // the ligature, and the é of one code point
    println("nfc {}, nfd {}, nfkc {}, nfkd {}", txt::normalize(s, txt::nfc).rune_count(),
            txt::normalize(s, txt::nfd).rune_count(), txt::normalize(s, txt::nfkc).rune_count(),
            txt::normalize(s, txt::nfkd).rune_count());
    println("{}", std::is_empty_v<txt::nfc_t>);
}
```

Output:

```text
nfc 5, nfd 6, nfkc 6, nfkd 7
true
```

## See also

- [normalize](normalize.md): the text in a form
- [is_normalized](is_normalized.md): whether it is in it already
- [txt](README.md)
