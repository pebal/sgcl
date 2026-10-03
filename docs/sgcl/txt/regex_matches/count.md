[sgcl](../../README.md) › [txt](../README.md) › [regex_matches](README.md)

# sgcl::txt::regex_matches::count

```cpp
size_type count() const noexcept;
```

Returns the number of matches: the range is walked and counted, not stored, so each call walks it again. It is
[regex::count](../regex/count.md) of the same text.

## Parameters

None.

## Return value

The number of matches; 0 for a range made by the default constructor.

## Complexity

Linear in the length of the text times the length of the pattern.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto vowels = txt::regex("(?i)[aeiouyąęó]").all("Żółta Łódź");
    println("{}", vowels.count());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md): whether there is no match
- [sgcl::txt::regex_matches](README.md)
