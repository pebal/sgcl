[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::text

```cpp
string text() const;
```

Returns the bytes held, from the first not yet read to the end, as a [string](../../core/string/README.md), a copy, without
taking them. Unlike [data](data.md), the string is the bytes as they were at the call, whatever is written after.
The bytes are not checked: a string holds any bytes.

## Parameters

None.

## Return value

A string of the bytes held; the empty string for an empty buffer.

## Complexity

Linear in the bytes held.

## Exceptions

`length_error` when the bytes held pass 4 GiB, the most a string holds.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    out.write("first");
    string before = out.text();
    out.write(", second");
    println("[{}] [{}]", before, out.text());
}
```

Output:

```text
[first] [first, second]
```

## See also

- [data](data.md): the bytes held, as a view
- [read_all_text](../mixin/reader/read_all_text.md): the bytes taken out as a string
- [sgcl::io::buffer](README.md)
