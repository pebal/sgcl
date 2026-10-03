[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](README.md)

# sgcl::net::http::headers::begin

```cpp
iterator begin() const noexcept;
```

Returns an iterator to the first field, in the order of the wire or of the program's `set` and `add`. The iterator is
an input iterator: `*it` makes a `pair<string, string>` of the name, as it was written, and the value, each time it
is read, so a loop over the fields takes them by value (`for (auto [name, value] : h)`). An iterator is valid until
the list is changed.

## Parameters

None.

## Return value

An iterator to the first field; equal to [end](end.md) when the list is empty.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    h.add("Host", "example.com").add("accept", "*/*");
    for (auto it = h.begin(); it != h.end(); ++it) {
        auto [name, value] = *it;
        println("{} = {}", name, value);
    }
}
```

Output:

```text
Host = example.com
accept = */*
```

## See also

- [end](end.md): the end of the fields
- [sgcl::net::http::headers](README.md)
