[sgcl](../../README.md) › [txt](../README.md) › [words](../words.md)

# sgcl::txt::words::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../words-iterator.md) to the first word, the runs before it passed over. For a text with no word
it equals [end](end.md).

## Parameters

None.

## Return value

An iterator to the first element.

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
    auto it = txt::words("  -- Ala ma kota").begin();
    println("[{}] at {}", *it, it.pos());
}
```

Output:

```text
[Ala] at 5
```

## See also

- [end](end.md): the iterator past the last element
- [sgcl::txt::words](../words.md)
