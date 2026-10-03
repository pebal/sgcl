[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](README.md)

# sgcl::txt::collated_matches::text

```cpp
slice<const char> text() const noexcept;
```

Returns the text the range walks, as a slice of the string the range holds: the elements are slices of it and the
positions of the iterator are bytes of it. A piece of a text or a C text is the copy the range holds.

## Parameters

None.

## Return value

The text; an empty slice for a range made by the default constructor.

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
    txt::collator primary{txt::strength::primary};
    string line = "#7 Résumé";
    txt::collated_searcher resume(primary, "resume");
    txt::collated_matches matches(primary, line.as_slice(3), resume);
    println("[{}] {}", matches.text(), matches.begin().pos());
}
```

Output:

```text
[Résumé] 0
```

## See also

- [pattern](pattern.md): the pattern
- [sgcl::txt::collated_matches](README.md)
