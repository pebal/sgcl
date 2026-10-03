[sgcl](../../README.md) › [txt](../README.md) › [collated_searcher](../collated_searcher.md)

# sgcl::txt::collated_searcher::by

```cpp
const collator& by() const noexcept;
```

Returns the collator the pattern was weighed by, the searcher's own copy of it. A text asks whether it is equal to
its own collator ([operator==](../collator/operator_cmp.md)) before it uses the weights; where it is not, the text
weighs the pattern again.

## Parameters

None.

## Return value

The collator.

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
    txt::collator czech(txt::locale("cs"));
    txt::collated_text text(czech, "chata");
    println("{} {}", text.searcher("ch").by() == czech,
            txt::collated_searcher(txt::collator(), "ch").by() == czech);
}
```

Output:

```text
true false
```

## See also

- [collator::operator==](../collator/operator_cmp.md)
- [sgcl::txt::collated_searcher](../collated_searcher.md)
