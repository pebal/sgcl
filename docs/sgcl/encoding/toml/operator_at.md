[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::operator[]

```cpp
toml operator[](size_t index) const noexcept;                               // (1)
toml operator[](const string& key) const noexcept;                          // (2)
template<size_t N> toml operator[](const char (&key)[N]) const noexcept;
```

A value inside, a copy of the handle; an empty table when there is none (and for a value of another kind), so
lookups chain: `t["server"]["port"]`.

1. An array's element at the index; an empty table past the end.
2. A table's value of the key; the literal's overload makes `t["key"]` unambiguous.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the place |
| `key` | the key |

## Return value

The value, or an empty table.

## Complexity

Linear in the members of a table; constant for an array.

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
    println(config["server"]["port"].as_int(0));
    println(config["route"][1]["path"].as_string("?"));
    println(config["missing"]["deeper"].empty());
}
```

Output:

```text
8080
/b
true
```

## See also

- [contains](contains.md)
- [sgcl::encoding::toml](README.md)
