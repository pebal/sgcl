[sgcl](../../README.md) › [core](../README.md) › [string](../string/README.md) › [pieces](README.md)

# sgcl::string::pieces::empty

```cpp
bool empty() const noexcept;
```

Checks whether the range has no piece, by `begin() == end()`: it searches for the first piece. A `split` has no piece
only of the empty string, since a string in which the separator does not occur is one piece; a `fields` has none
also of a string of white space alone.

## Parameters

None.

## Return value

`true` when there is no piece, `false` otherwise.

## Complexity

The complexity of [begin](begin.md): one search, for the first piece.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", string().split(',').empty(), string("abc").split(',').empty());
    println("{} {}", string(" \t ").fields().empty(), string(" word ").fields().empty());
}
```

Output:

```text
true false
true false
```

## See also

- [begin](begin.md), [end](end.md): the iterators of the range
- [sgcl::string::pieces](README.md)
