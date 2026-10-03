[sgcl](../../README.md) › [txt](../README.md) › [collated_searcher](../collated_searcher.md)

# sgcl::txt::collated_searcher::pattern

```cpp
const string& pattern() const noexcept;
```

Returns the pattern as it was given, before it was weighed: what a text weighs again when the searcher belongs to
another collator than the text's.

## Parameters

None.

## Return value

The pattern.

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
    txt::collated_searcher s(txt::collator(txt::strength::primary), "Łódź");
    println("{}: {} elements", s.pattern(), s.size());
}
```

Output:

```text
Łódź: 4 elements
```

## See also

- [by](by.md): the collator
- [sgcl::txt::collated_searcher](../collated_searcher.md)
