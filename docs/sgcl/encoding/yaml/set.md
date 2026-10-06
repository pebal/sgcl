[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::set

```cpp
yaml set(const yaml& key, const yaml& value) const noexcept;
```

A new node: a mapping with the key set to the value, in its place when the key is there, at the end when it is not;
a sequence with the element at an integer index replaced, or added at one past the end. The node itself never
changes; anything else, an index further out among them, gives the node as it is. The tag is kept.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, or the index of a sequence |
| `value` | the value |

## Return value

The new node.

## Complexity

Linear in the size of the mapping or the sequence.

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
    print(config.set("debug", true).set("paths", config["paths"].set(2, "/c")).to_string());
}
```

Output:

```text
server:
  host: example.com
  port: 8080
  tls: true
paths:
  - /a
  - /b
  - /c
debug: true
```

## See also

- [erase](erase.md)
- [push_back](push_back.md)
- [sgcl::encoding::yaml](README.md)
