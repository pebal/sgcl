[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::operator[]

```cpp
yaml operator[](size_t index) const noexcept;                               // (1)
yaml operator[](const string& key) const noexcept;                          // (2)
template<size_t N> yaml operator[](const char (&key)[N]) const noexcept;
yaml operator[](const yaml& key) const noexcept;                            // (3)
```

A node inside, a copy of the handle; null when there is none (and for a node of another kind).

1. A sequence's element at the index; null past the end.
2. A mapping's value of the string key; the literal's overload makes `y["key"]` unambiguous.
3. A mapping's value of the key of any kind, by deep equality: `y[yaml(1)]` for the integer key 1 (`y[1]` is (1)), a key that is a sequence.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the place |
| `key` | the key |

## Return value

The node, or null.

## Complexity

Linear in the members of a mapping; constant for a sequence.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml config = encoding::yaml::parse(R"(
server:
  host: example.com
  port: 8080
  tls: true
paths: [/a, /b]
)").value();
    println(config["server"]["port"].as_int(0));
    println(config["paths"][1].as_string("?"));
    println(config["missing"]["deeper"].is_null());
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
- [sgcl::encoding::yaml](README.md)
