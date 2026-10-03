[sgcl](../../README.md) › [io](../README.md) › [multi_reader](../multi_reader.md)

# sgcl::io::multi_reader::multi_reader

```cpp
explicit multi_reader(vector<io::reader> readers) noexcept;
```

Constructs a reader of `readers`, one after another, the first one first. The vector is taken by value and moved
in: a vector's move takes its buffer over, where a copy would copy every reader. A braced list of streams makes the
vector in the call, each converted to an [io::reader](../reader.md). Nothing is read until the first read; an empty
vector is a stream at its end at once.

## Parameters

| Parameter | Description |
|---|---|
| `readers` | the readers, in the order they are read |

## Complexity

Constant: the vector is moved in.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<io::reader> parts;
    for (string part : {"one ", "two ", "three"}) {
        parts.push_back(io::buffer(part));
    }
    io::multi_reader all(std::move(parts));
    io::multi_reader none({});
    println("[{}] [{}]", *all.read_all_text(), *none.read_all_text());
}
```

Output:

```text
[one two three] []
```

## See also

- [read, async_read](read.md)
- [sgcl::io::multi_reader](../multi_reader.md)
