[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::backwards

```cpp
bool backwards() const noexcept;
```

Returns whether the collator settled on comparing the accents from the end of the word rather than from its start,
CLDR's `kb` — what Canadian French asks for: `cote côte coté côté` rather than `cote coté côte côté`. It is the
caller's answer where the [options](../collator-options.md) gave one, the language's where they did not — Church Slavonic
asks for it.

## Parameters

None.

## Return value

`true` when the accents are compared from the end, `false` otherwise.

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
    vector<string> words = {"côté", "cote", "coté", "côte"};
    txt::collator canadian{{.backwards = true}};
    words.sort(canadian);
    println("{} {}", canadian.backwards(), words);
    words.sort(txt::collator());
    println("{}", words);
}
```

Output:

```text
true ["cote", "côte", "coté", "côté"]
["cote", "coté", "côte", "côté"]
```

## See also

- [options](../collator-options.md)
- [sgcl::txt::collator](../collator.md)
