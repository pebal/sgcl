[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::rbegin, crbegin

```cpp
const_reverse_iterator rbegin() const noexcept;     // (1)
const_reverse_iterator crbegin() const noexcept;    // (2)
```

Returns a reverse iterator to the last character: `std::reverse_iterator` over [end()](end.md), read only. (2) is (1)
under the name of `std`. For the empty string it equals [rend()](rend.md).

## Parameters

None.

## Return value

A reverse iterator to the last character.

## Complexity

Constant.

## Exceptions

None.

## Notes

A reverse walk of a `string` goes byte by byte: the bytes of a code point outside ASCII come in reverse order, and a
UTF-8 text reversed this way is not valid UTF-8.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string word = "stressed";
    string reversed(word.rbegin(), word.rend());
    println("{} {}", reversed, *word.crbegin());
}
```

Output:

```text
desserts d
```

## See also

- [rend, crend](rend.md): a reverse iterator before the first character
- [begin, cbegin](begin.md): an iterator to the first character
- [sgcl::string](../string.md)
