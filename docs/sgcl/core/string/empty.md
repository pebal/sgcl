[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::empty

```cpp
bool empty() const noexcept;
```

Checks whether the string has no characters. The empty string is null, the word without an object: a string made of
no characters, from whichever source (an empty literal, an empty view, `(0, c)`, a `substr` past the last character,
the `trim` of white space), allocates nothing and is that null word, so the check is a test of the word.

## Parameters

None.

## Return value

`true` when the string has no characters, `false` otherwise.

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
    string none;
    string literal = "";
    string spaces = "   ";
    string trimmed = spaces.trim();
    println("{} {} {} {}", none.empty(), literal.empty(), spaces.empty(), trimmed.empty());
    println("{} {}", literal.object() == nullptr, trimmed.object() == nullptr);
}
```

Output:

```text
true true false true
true true
```

## See also

- [size](size.md): the number of characters
- [object](object.md): the address of the string's object, null when empty
- [sgcl::string](../string.md)
