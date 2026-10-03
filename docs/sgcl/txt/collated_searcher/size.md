[sgcl](../../README.md) › [txt](../README.md) › [collated_searcher](README.md)

# sgcl::txt::collated_searcher::size

```cpp
size_t size() const noexcept;
```

Returns the number of elements of the pattern the collator looks at, which is not the length of the pattern in
letters: an `æ` the root weighs as two letters counts twice, and an accent at primary strength counts not at all.

## Parameters

None.

## Return value

The number of elements searched for.

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
    txt::collator primary{txt::strength::primary};
    println("{} {} {}", txt::collated_searcher(primary, "æ").size(),
            txt::collated_searcher(primary, "é").size(),
            txt::collated_searcher(primary, "\u0301").size());
}
```

Output:

```text
2 1 0
```

## See also

- [empty](empty.md): whether there is nothing to look for
- [sgcl::txt::collated_searcher](README.md)
