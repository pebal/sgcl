[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::begin, cbegin

```cpp
const_iterator begin() const noexcept;     // (1)
const_iterator cbegin() const noexcept;    // (2)
```

Returns an iterator to the first character. The iterator is `const CharT*`, a pointer into the string's object: a
contiguous, read-only iterator, since a string is never modified. (2) is (1) under the name of `std`. For the empty
string the iterator points at a terminator of its own and equals [end()](end.md).

## Parameters

None.

## Return value

An iterator to the first character, `data()`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The iterator is valid while some string holds the object, as the pointer of [data](data.md) is. In a `string` it walks
the bytes; `runes()` walks the code points ([runes](../runes/README.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

int main() {
    string text = "hello, world";
    println("{} {}", *text.begin(), std::count(text.begin(), text.end(), 'o'));

    string empty;
    println("{}", empty.cbegin() == empty.cend());
}
```

Output:

```text
h 2
true
```

## See also

- [end, cend](end.md): an iterator past the last character
- [rbegin, crbegin](rbegin.md): a reverse iterator to the last character
- [sgcl::string](README.md)
