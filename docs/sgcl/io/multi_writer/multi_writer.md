[sgcl](../../README.md) › [io](../README.md) › [multi_writer](../multi_writer.md)

# sgcl::io::multi_writer::multi_writer

```cpp
explicit multi_writer(vector<io::writer> writers) noexcept;
```

Constructs a writer to every one of `writers`, in their order. The vector is taken by value and moved in: a
vector's move takes its buffer over, where a copy would copy every writer. A braced list of streams makes the
vector in the call, each converted to an [io::writer](../writer.md). An empty vector is a writer that takes
everything and writes it nowhere.

## Parameters

| Parameter | Description |
|---|---|
| `writers` | the writers, in the order they are written to |

## Complexity

Constant: the vector is moved in.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<io::writer> copies;
    io::buffer first, second;
    copies.push_back(first);
    copies.push_back(second);
    io::multi_writer out(std::move(copies));
    out.write("twice");
    println("{} {}", first.text(), second.text());
}
```

Output:

```text
twice twice
```

## See also

- [write, async_write](write.md)
- [sgcl::io::multi_writer](../multi_writer.md)
