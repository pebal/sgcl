[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the error the [lines](lines.md) (or `async_lines`) of this reader ended on, if any, as Go's `Scanner.Err()`:
a range-for cannot return an error, so the loop ends and the error is read after it. It is cleared when the lines
start again. The other operations give their errors as their results and leave it as it is.

## Parameters

None.

## Return value

The error the lines ended on; an empty `optional` when they ended at the end of the stream, or were never iterated.
The reference is to the reader's state, valid while a handle holds it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("short\nthis one is far too long\nnever read\n"));
    in.set_max_line(10);
    for (auto line : in.lines()) {
        println("{}", line);
    }
    println("{}", in.last_error() ? in.last_error()->message() : "the end");
}
```

Output:

```text
short
read_line: line too long
```

## See also

- [lines](lines.md): the lines as a range
- [error](../error.md): what it holds
- [sgcl::io::buffered_reader](../buffered_reader.md)
