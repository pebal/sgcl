[sgcl](../../README.md) › [core](../README.md) › [string](../string.md) › [pieces](../string-pieces.md)

# sgcl::string::pieces::text

```cpp
const basic_string& text() const noexcept;
```

Returns the string the pieces are of: the one `split` or `fields` was called on, held by the range. The reference is
to the range's own string, valid while the range lives; a copy of it is the same object, one word.

## Parameters

None.

## Return value

A reference to the string the pieces are of.

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
    string line = "x=1;y=2";
    string::pieces pairs = line.split(';');
    println("{} {}", pairs.text(), pairs.text().object() == line.object());
}
```

Output:

```text
x=1;y=2 true
```

## See also

- [begin](begin.md): an iterator to the first piece
- [sgcl::string::pieces](../string-pieces.md)
