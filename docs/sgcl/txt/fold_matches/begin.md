[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](../fold_matches.md)

# sgcl::txt::fold_matches::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../fold_matches-iterator.md) to the first occurrence of the pattern in the text, searched for
when this is called; for a text without one, an empty pattern and an empty range it equals [end](end.md).

## Parameters

None.

## Return value

An iterator to the first occurrence.

## Complexity

A scan of the mapped text to the first occurrence: linear on ordinary text, the text times the pattern at worst.

## Exceptions

None.

## Notes

The iterator holds the tracked object with the mapped text, not the range: it stays valid after the range is gone.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto first = txt::fold_matches("Ein Gruß, ein GRUSS", "gruss").begin();
    println("[{}] at {}, {} bytes", *first, first.pos(), first.size());
}
```

Output:

```text
[Gruß] at 4, 5 bytes
```

## See also

- [end](end.md): the iterator past the last occurrence
- [sgcl::txt::fold_matches, normalized_matches](../fold_matches.md)
