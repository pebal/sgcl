[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::to_json

```cpp
json to_json() const noexcept;
```

The node as [json](../json/README.md): the kinds as they are (an integer past 64 bits a float), `.inf` and `.nan`
null, a mapping's key that is not a string its YAML text (`1`, `[a, b]`), tags dropped.

## Parameters

None.

## Return value

The JSON value.

## Complexity

Linear in the size of the node.

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
    println(config.to_json().to_string());
}
```

Output:

```text
{"server":{"host":"example.com","port":8080,"tls":true},"paths":["/a","/b"]}
```

## See also

- [from_json](from_json.md)
- [sgcl::encoding::yaml](README.md)
