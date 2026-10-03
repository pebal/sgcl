[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::restriction_level_of

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    restriction_level restriction_level_of(const string& text) noexcept;
}
```

Returns where the text stands on the ladder of [UTS #39](https://www.unicode.org/reports/tr39/) §5.2
([restriction_level](restriction_level.md)): `unrestricted` when a code point of it is not allowed in an identifier
([identifier_status_of](identifier_status_of.md)), `ascii_only` when every code point is ASCII, and otherwise the
narrowest rung its scripts reach. A caller names the rung it accepts; the specification suggests
`moderately_restrictive` for a registry open to the world and `highly_restrictive` where a mistaken name costs
something.

**An empty text is `unrestricted`**, which is the least safe rung, and that is a decision rather than an accident:
an empty name is not a name, and a function whose answer is acted on by refusing everything above a rung has to fail
towards refusing.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The rung of the text.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Notes

- **A mixed-script name is not necessarily an attack and a single-script one is not necessarily safe.** A wholly
  Cyrillic `"расчёт"` is single script and perfectly honest; a wholly Cyrillic `"расс"` written to be read as Latin
  `"pacc"` is single script too. The level is evidence, not a verdict.
- **Script_Extensions is not used** ([is_single_script](is_single_script.md)): a code point that is `Common` although
  only a few scripts use it counts as belonging to all of them, so the rung answered here may be narrower than the
  specification's, never wider.
- A ninth distinct script settles every rung below the last one: it is not one script, it is not one of the three
  writing systems, which are four scripts at their widest, and it is not Latin with one other, so the text is
  `minimally_restrictive`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    const char* names[] = {"ascii_only", "single_script", "highly_restrictive",
                           "moderately_restrictive", "minimally_restrictive", "unrestricted"};
    for (auto s : {"paypal", "wartość", "変数", "変数abc", "abcא", "раypal", "na me", ""}) {
        println("[{}] {}", s, names[size_t(txt::restriction_level_of(s))]);
    }
}
```

Output:

```text
[paypal] ascii_only
[wartość] single_script
[変数] single_script
[変数abc] highly_restrictive
[abcא] moderately_restrictive
[раypal] minimally_restrictive
[na me] unrestricted
[] unrestricted
```

## See also

- [restriction_level](restriction_level.md): the rungs
- [is_highly_restrictive](is_highly_restrictive.md), [is_moderately_restrictive](is_moderately_restrictive.md): the
  two rungs asked for by name
- [is_confusable](is_confusable.md): a name that looks like another
- [txt](README.md)
