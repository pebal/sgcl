[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::size, length

```cpp
size_type size() const noexcept;      // (1)
size_type length() const noexcept;    // (2)
```

Returns the number of characters, `CharT`s, without the terminator. In a `string`, whose text is UTF-8, that is the
number of bytes, not of letters: `"zażółć"` has 10 bytes and 6 code points. `length()` is the same number under
the other name of `std::string`. The number of code points is `rune_count()` ([mixin::text](../mixin/text/README.md)).

The length is kept in the string's object, 32 bits beside the hash, so it is read rather than counted; the empty
string has no object and its size is 0.

## Parameters

None.

## Return value

The number of characters.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string word = "hello";
    string polish = "zażółć";
    string empty;
    println("{} {} {}", word.size(), polish.size(), empty.size());
    println("{} {}", polish.rune_count(), polish.length());
}
```

Output:

```text
5 10 0
6 10
```

## See also

- [empty](empty.md): checks whether the string has no characters
- [max_size](max_size.md): the largest number of characters a string holds
- [runes](../runes/README.md): the code points of a text
- [sgcl::string](README.md)
