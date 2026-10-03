[sgcl](../../README.md) › [txt](../README.md) › [words](../words.md)

# sgcl::txt::words::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text has no word: whether it is empty or holds only spaces and punctuation. It is `begin() ==
end()`, the walk made to the first word.

## Parameters

None.

## Return value

`true` when there is no word.

## Complexity

Linear in the bytes up to the end of the first word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::words("").empty(), txt::words(" -- !").empty(),
            txt::words("a").empty());
}
```

Output:

```text
true true false
```

## See also

- [count](count.md): the number of words
- [sgcl::txt::words](../words.md)
