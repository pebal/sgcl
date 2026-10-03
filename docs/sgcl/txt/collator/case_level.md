[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::case_level

```cpp
bool case_level() const noexcept;
```

Returns whether the collator settled on the case as a level of its own, between the accents and the third level —
CLDR's `caseLevel` (`kc`): at primary strength with it, `resume` and `résumé` are one word and `RESUME` is not. It
is the caller's answer where the [options](../collator-options.md) gave one, the language's where they did not — Church
Slavonic asks for it.

## Parameters

None.

## Return value

`true` when the case is a level of its own, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator cased{{.strength = txt::strength::primary, .case_level = true}};
    println("{} {} {}", cased.case_level(), cased.equal("resume", "résumé"),
            cased.equal("resume", "RESUME"));
}
```

Output:

```text
true true false
```

## See also

- [options](../collator-options.md)
- [sgcl::txt::collator](README.md)
