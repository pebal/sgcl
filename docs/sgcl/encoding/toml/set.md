[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::set

```cpp
toml set(const string& key, const toml& value) const noexcept;    // (1)
toml set(size_t index, const toml& value) const noexcept;         // (2)
```

A new value; the value itself never changes.

1. A table with the key set to the value, in its place when the key is there, at the end when it is not; any other
   value as it is.
2. An array with the element at the index replaced, or added at one past the end; any other value, an index further
   out among them, as it is.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |
| `index` | the place |
| `value` | the value |

## Return value

The new value.

## Complexity

Linear in the size of the table or the array.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml config = encoding::toml::parse(R"(
title = "app"

[server]
host = "example.com"
port = 8080
started = 2026-10-06T09:30:00+02:00

[[route]]
path = "/a"

[[route]]
path = "/b"
)").value();
    print(config.set("debug", true).set("title", "renamed").erase("route").to_string());
}
```

Output:

```text
title = "renamed"
debug = true

[server]
host = "example.com"
port = 8080
started = 2026-10-06T09:30:00+02:00
```

## See also

- [erase](erase.md)
- [push_back](push_back.md)
- [sgcl::encoding::toml](README.md)
