[sgcl](../../README.md) › [txt](../README.md) › [match](../match.md)

# sgcl::txt::match::empty

```cpp
bool empty() const noexcept;
```

Checks whether the match has no width, `begin_at() == end_at()`: what `x*`, `^` or `\b` find. An empty match is a
match all the same; whether there was one at all is the `optional` that [regex::find](../regex/find.md) returns.

## Parameters

None.

## Return value

`true` when the match covers no bytes, `false` otherwise.

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
    txt::regex digits("\\d*");
    println("{} {}", digits.find("abc")->empty(), digits.find("42")->empty());
}
```

Output:

```text
true false
```

## See also

- [text](text.md): the bytes of the match
- [sgcl::txt::match](../match.md)
