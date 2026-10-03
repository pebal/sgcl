[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](README.md)

# sgcl::txt::collated_matches::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../collated_matches-iterator/README.md) to the first occurrence of the pattern in the text, searched
for when this is called; for a text without one, an empty pattern and an empty range it equals [end](end.md).

## Parameters

None.

## Return value

An iterator to the first occurrence.

## Complexity

A scan of the weighed text to the first occurrence: linear on ordinary text, the text times the pattern at worst.

## Exceptions

None.

## Notes

The iterator holds the tracked object with the weighed text, not the range: it stays valid after the range is gone.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator primary{txt::strength::primary};
    auto first = txt::collated_matches(primary, "Le CV, le résumé", "resume").begin();
    println("[{}] at {}, {} bytes", *first, first.pos(), first.size());
}
```

Output:

```text
[résumé] at 10, 8 bytes
```

## See also

- [end](end.md): the iterator past the last occurrence
- [sgcl::txt::collated_matches](README.md)
