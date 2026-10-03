[sgcl](../../README.md) › [io](../README.md) › [reader](README.md)

# sgcl::io::reader::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the reader holds a stream. A default-constructed reader holds none, and so does one made of an empty
handle (a default-constructed [buffered_reader](../buffered_reader/README.md)) or of a null pointer to a handle or to a
stream of the program's.

## Parameters

None.

## Return value

`true` when the reader holds a stream, `false` when it is empty.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::reader none;
    io::buffered_reader no_source;
    io::reader from_empty = no_source;
    tracked_ptr<io::buffer> nothing;
    io::reader from_null = nothing;
    io::reader memory = io::buffer("bytes");
    println("{} {} {} {}", bool(none), bool(from_empty), bool(from_null), bool(memory));
}
```

Output:

```text
false false false true
```

## See also

- [(constructor)](reader.md)
- [sgcl::io::reader](README.md)
