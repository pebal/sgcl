[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::tagged

```cpp
static yaml tagged(const string& tag, const yaml& value) noexcept;
```

The node under an application's tag: `!Ref`, `!Sub`, a URI such as `tag:example.com,2026:point`. The node keeps its
kind and gains the [tag](tag.md), which [to_string](to_string.md) writes before it (a tag of another form as
`!<...>`). A node tagged again has the new tag in place of the old one. The core schema's tags (`!!int`, `!!str`)
are no tags of a node: a node of the kind is made for them.

## Parameters

| Parameter | Description |
|---|---|
| `tag` | the tag, as [tag](tag.md) gives it |
| `value` | the node |

## Return value

The node.

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
    encoding::yaml bucket = encoding::yaml::mapping({
        {"Name", encoding::yaml::tagged("!Ref", "BucketName")},
        {"Arn", encoding::yaml::tagged("!GetAtt", encoding::yaml::sequence({"Bucket", "Arn"}))}});
    print(bucket.to_string());
}
```

Output:

```text
Name: !Ref BucketName
Arn: !GetAtt
  - Bucket
  - Arn
```

## See also

- [tag](tag.md)
- [sgcl::encoding::yaml](README.md)
