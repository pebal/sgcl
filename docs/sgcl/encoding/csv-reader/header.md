[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [reader](README.md)

# sgcl::encoding::csv::reader::header

```cpp
slice<const string> header() const noexcept;
```

The names of the columns, the fields of the record [read_header](read_header.md) read, or that
[read\<T\>](read.md) took for the header, in their order.

## Parameters

None.

## Return value

The names, as a [slice](../../core/slice/README.md) that keeps them alive; empty when no header was read.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("id;name;e-mail\n1;Ann;ann@example.com\n", {.separator = ';'});
    println("{}", r.header().size());
    r.read_header();
    for (const string& name : r.header()) {
        println("{}", name);
    }
}
```

Output:

```text
0
id
name
e-mail
```

## See also

- [read_header](read_header.md): the header read
- [sgcl::encoding::csv::reader](README.md)
