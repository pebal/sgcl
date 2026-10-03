[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::operator==

```cpp
bool operator==(const collator&) const noexcept = default;
```

Checks whether two collators put text in the same order: the same language's table, the same strength and the same
settings settled on. It is what a prepared searcher and a prepared text have to agree on before one can answer
about the other ([collated_text::find](../collated_text/find.md)). Every member takes part, and the two that are
worked out rather than given follow from the ones that are, so the comparison costs a few words and cannot say no
where the rest said yes. `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| The unnamed parameter | the collator compared with this one |

## Return value

`true` when the two collators order every text alike, `false` otherwise.

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
    txt::collator cs(txt::locale("cs"));
    println("{} {}", cs == txt::collator(txt::locale("cs-CZ")),
            cs == txt::collator(txt::locale("pl")));
    println("{}", cs != txt::collator(txt::locale("cs"), txt::strength::primary));
}
```

Output:

```text
true false
true
```

## See also

- [collated_searcher::by](../collated_searcher/by.md): the collator a pattern was weighed by
- [sgcl::txt::collator](../collator.md)
