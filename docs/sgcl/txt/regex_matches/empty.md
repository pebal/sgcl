[sgcl](../../README.md) › [txt](../README.md) › [regex_matches](README.md)

# sgcl::txt::regex_matches::empty

```cpp
bool empty() const noexcept;
```

Checks whether the pattern has no match in the text, `begin() == end()`: one search, which stops at the first
match. A match of no width counts, so a pattern that can match nothing is never empty over a text.

## Parameters

None.

## Return value

`true` when there is no match, or the range was made by the default constructor; `false` otherwise.

## Complexity

One search: linear in the length of the text up to the first match times the length of the pattern.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex digits("\\d+");
    println("{} {}", digits.all("bez cyfr").empty(), digits.all("1 cyfra").empty());
    println("{}", txt::regex("\\d*").all("bez cyfr").empty());
}
```

Output:

```text
true false
false
```

## See also

- [count](count.md): the number of matches
- [sgcl::txt::regex_matches](README.md)
