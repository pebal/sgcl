[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::shifts_punctuation

```cpp
bool shifts_punctuation() const noexcept;
```

Returns whether the collator settled on [punctuation::shifted](../punctuation.md), CLDR's `alternate` (`ka`):
punctuation, spaces and symbols moved to the fourth level. It is the caller's answer where the
[options](../collator-options.md) gave one, the language's where they did not — Thai asks for it.

The `optional` of the options says "not given, so take the language's", which is a question about what was asked
for; this is the answer, and an answer is a `bool`. A name that reads as a question keeps the type names free: a
member called `punctuation` would hide the type of that name inside the class.

## Parameters

None.

## Return value

`true` when punctuation is shifted, `false` when it is counted.

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
    txt::collator thai(txt::locale("th"));
    txt::collator counted(txt::locale("th"), {.punctuation = txt::punctuation::counted});
    println("{} {} {}", txt::collator().shifts_punctuation(), thai.shifts_punctuation(),
            counted.shifts_punctuation());
}
```

Output:

```text
false true false
```

## See also

- [punctuation](../punctuation.md), [options](../collator-options.md)
- [sgcl::txt::collator](README.md)
