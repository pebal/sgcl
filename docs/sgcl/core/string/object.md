[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::object

```cpp
const void* object() const noexcept;
```

Returns the address of the string's object: its identity, where `==` compares the characters. A copy of a string is
the same object; two strings made of the same characters are two objects, equal and not identical; the empty string
has no object, and its address is null.

The operations that return a string return the same object when there is nothing to change: `trim` of a string
without white space at its ends, `replace` of a text that does not occur, `to_lower` of a string with no upper-case
letter, `substr` and `repeat(1)` of the whole, a string made of a slice that is the whole of a string. A result may
therefore be compared by `object()`, a comparison of two words, as by `==`.

## Parameters

None.

## Return value

The address of the object, or `nullptr` for the empty string.

## Complexity

Constant.

## Exceptions

None.

## Notes

The object holds the length and the hash, eight bytes together, the characters and a terminator, of exactly that size
rounded to four: the object of `"alice"` is 16 bytes. The address is valid while some string holds the object; it is
for comparing and is not printed, as it differs from run to run.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string name = "alice";
    string copy = name;
    string other("alice");
    println("{} {}", copy.object() == name.object(), other.object() == name.object());
    println("{}", other == name);

    string lower = name.to_lower();  // no letter to change
    println("{} {}", lower.object() == name.object(), string().object() == nullptr);
}
```

Output:

```text
true false
true
true true
```

## See also

- [operator==, operator\<=\>](operator_cmp.md): the comparisons by the characters
- [hash](hash.md): the hash of the characters, kept in the object
- [sgcl::string](README.md)
