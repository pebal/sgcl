[sgcl](../../README.md) › [txt](../README.md) › [regex_matches](README.md)

# sgcl::txt::regex_matches::begin

```cpp
iterator begin() const noexcept;
```

Returns an iterator to the first match of the pattern in the text, searched for when this is called; for a text
with no match, and for an empty range, it equals [end](end.md). Each call searches again.

## Parameters

None.

## Return value

An iterator to the first match.

## Complexity

One search: linear in the length of the text up to the first match times the length of the pattern.

## Exceptions

None.

## Notes

The iterator holds the compiled pattern and the text, not the range: it stays valid after the range is gone. Its
`*` is a `const match&` of the match it stands on, which `++` replaces with the next one.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto it = txt::regex("\\d+").all("a1 b22 c333").begin();
    ++it;
    println("{} at {}", it->text(), it->begin_at());
}
```

Output:

```text
22 at 4
```

## See also

- [end](end.md): the iterator past the last match
- [sgcl::txt::regex_matches](README.md)
