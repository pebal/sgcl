[sgcl](../../README.md) › [txt](../README.md) › [regex_matches](README.md)

# sgcl::txt::regex_matches::text

```cpp
const slice<const char>& text() const noexcept;
```

Returns the text the range walks, as the slice it holds: the text it was given, or the copy of a C text. Every
match's [subject](../match/subject.md) is this slice.

## Parameters

None.

## Return value

The slice of the text; an empty slice for a range made by the default constructor.

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
    auto numbers = txt::regex("\\d+").all("3 koty, 2 psy");
    println("{} matches in \"{}\"", numbers.count(), numbers.text());
}
```

Output:

```text
2 matches in "3 koty, 2 psy"
```

## See also

- [match::subject](../match/subject.md): the same text, from a match
- [sgcl::txt::regex_matches](README.md)
