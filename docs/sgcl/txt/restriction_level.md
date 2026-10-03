[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::restriction_level

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class restriction_level : uint8_t {
        ascii_only,
        single_script,
        highly_restrictive,
        moderately_restrictive,
        minimally_restrictive,
        unrestricted,
    };
}
```

The ladder of [UTS #39](https://www.unicode.org/reports/tr39/) §5.2, from the narrowest rung to the widest, what
[restriction_level_of](restriction_level_of.md) answers. A caller picks the rung it will accept and refuses what is
above it; the specification suggests `moderately_restrictive` for a registry open to the world and
`highly_restrictive` where a mistaken name costs something. The enumerators are in the ladder's order, so
`level <= restriction_level::moderately_restrictive` asks for a rung or a narrower one.

| Value | Description |
|---|---|
| `ascii_only` | every code point is ASCII |
| `single_script` | one script, `Common` and `Inherited` aside |
| `highly_restrictive` | the scripts of one of the three writing systems that are more than one script, with Latin beside them: Latin, Han, Hiragana and Katakana (Japanese); Latin, Han and Bopomofo (Chinese); Latin, Han and Hangul (Korean) |
| `moderately_restrictive` | Latin and one other recommended script that is not Cyrillic, Greek or Cherokee, whose letters are the ones Latin is mistaken for |
| `minimally_restrictive` | every code point allowed, the scripts mixed freely |
| `unrestricted` | a code point that is not allowed, or an empty text |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"abc", "żółw", "abc漢字", "abcא", "abcж", "a b"}) {
        auto level = txt::restriction_level_of(s);
        bool taken = level <= txt::restriction_level::moderately_restrictive;
        println("{}: {}", s, taken ? "taken" : "refused");
    }
}
```

Output:

```text
abc: taken
żółw: taken
abc漢字: taken
abcא: taken
abcж: refused
a b: refused
```

## See also

- [restriction_level_of](restriction_level_of.md): the rung of a text
- [is_highly_restrictive](is_highly_restrictive.md), [is_moderately_restrictive](is_moderately_restrictive.md): the
  two rungs asked for by name
- [txt](README.md)
