[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::swap, sgcl::swap (sgcl::string)

```cpp
void swap(basic_string& o) noexcept;                                                       // (1)

namespace sgcl {
    template<class CharT, class Traits>
    void swap(basic_string<CharT, Traits>& l, basic_string<CharT, Traits>& r) noexcept;    // (2)
}
```

1. Exchanges the words of this string and `o`: each holds the other's object after, and no character is touched.
2. Exchanges the words of `l` and `r`, `l.swap(r)`. Found by the argument's type, so `swap(a, b)` written without a
   namespace calls it; `std::swap(a, b)` works as well, through the moves, which copy a word.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the string to exchange with |
| `l`, `r` | the strings to exchange |

## Return value

None.

## Complexity

Constant: two words.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string first = "alice", second = "bob";
    const void* alice = first.object();

    first.swap(second);
    println("{} {} {}", first, second, second.object() == alice);

    swap(first, second);
    println("{} {} {}", first, second, first.object() == alice);
}
```

Output:

```text
bob alice true
alice bob true
```

## See also

- [operator=](operator_assign.md): assigns another string
- [sgcl::string](README.md)
