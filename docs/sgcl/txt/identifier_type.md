[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::identifier_type

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class identifier_type : uint16_t {
        not_character = 0,
        deprecated        = 1 << 0,
        default_ignorable = 1 << 1,
        not_nfkc          = 1 << 2,
        not_xid           = 1 << 3,
        exclusion         = 1 << 4,
        obsolete          = 1 << 5,
        technical         = 1 << 6,
        uncommon_use      = 1 << 7,
        limited_use       = 1 << 8,
        inclusion         = 1 << 9,
        recommended       = 1 << 10,
    };

    constexpr identifier_type operator|(identifier_type a, identifier_type b) noexcept;
    constexpr identifier_type operator&(identifier_type a, identifier_type b) noexcept;
}
```

The `Identifier_Type` property of [UTS #39](https://www.unicode.org/reports/tr39/), what
[identifier_type_of](identifier_type_of.md) answers: what is wrong with a code point when it does not belong in a
name, or why it does. A code point carries a **set** of the types and not one of them, so the eleven are bits of one
word, read with `&`: `(identifier_type_of(c) & identifier_type::technical) != identifier_type::not_character`. The
twelfth, `Not_Character`, is the empty set, the value of every code point the file does not name.

| Value | Description |
|---|---|
| `not_character` | the empty set: unassigned, a private use, a surrogate, a noncharacter |
| `deprecated` | a deprecated code point |
| `default_ignorable` | a default ignorable code point, which is not drawn |
| `not_nfkc` | not in NFKC: a compatibility form |
| `not_xid` | not `XID_Continue`: no identifier holds it |
| `exclusion` | of a script excluded from identifiers, a historic one |
| `obsolete` | no longer in customary use |
| `technical` | for specialized use, not for ordinary text |
| `uncommon_use` | not in common use |
| `limited_use` | of a script in limited use |
| `inclusion` | punctuation allowed in identifiers, the hyphen and the apostrophe among them |
| `recommended` | recommended for identifiers |

## Rules

- `operator|` and `operator&` are the union and the intersection of two sets.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    using type = txt::identifier_type;
    for (char32_t c : {U'a', U'ſ', U'ǀ'}) {
        auto t = txt::identifier_type_of(c);
        bool recommended = (t & type::recommended) != type::not_character;
        bool not_nfkc = (t & type::not_nfkc) != type::not_character;
        bool technical = (t & type::technical) != type::not_character;
        println("[{}] recommended {}, not NFKC {}, technical {}", c, recommended, not_nfkc,
                technical);
    }
}
```

Output:

```text
[a] recommended true, not NFKC false, technical false
[ſ] recommended false, not NFKC true, technical false
[ǀ] recommended false, not NFKC false, technical true
```

## See also

- [identifier_type_of](identifier_type_of.md): the type of a code point
- [identifier_status](identifier_status.md): whether it is allowed
- [txt](README.md)
