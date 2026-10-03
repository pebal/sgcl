[sgcl](../../README.md) › [core](../README.md) › [string](../string.md) › [pieces](../string-pieces.md)

# sgcl::string::pieces::end

```cpp
iterator end() const noexcept;
```

Returns the iterator past the last piece: an iterator from [begin](begin.md), advanced past the last piece, compares
equal to it. It searches nothing and refers to no piece; it may not be dereferenced.

## Parameters

None.

## Return value

The iterator past the last piece.

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
    string csv = "a,,b";
    string::pieces cells = csv.split(',');

    int count = 0;
    for (auto it = cells.begin(); it != cells.end(); ++it) {
        print("[{}]", *it);
        ++count;
    }
    println(" {}", count);
}
```

Output:

```text
[a][][b] 3
```

## See also

- [begin](begin.md): an iterator to the first piece
- [sgcl::string::pieces](../string-pieces.md)
