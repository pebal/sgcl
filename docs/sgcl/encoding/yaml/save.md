[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::save, async_save

```cpp
expected<void, error> save(const string& path) const;                         // (1)
async::task<expected<void, error>> async_save(string path) const noexcept;    // (2)
```

The node's [to_string](to_string.md) into a file, made or written over.

1. Written now.
2. (1) for a task: the node copied into it, the file written on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

Nothing, or the [error](../error/README.md) `io`, without a place, for a file that does not open or write.

## Complexity

Linear in the size of the node as written.

## Exceptions

- (1) What [to_string](to_string.md) throws, and what a write of the file throws.
- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml config = encoding::yaml::mapping({{"name", "app"}, {"ports", encoding::yaml::sequence({80, 443})}});
    config.save("app.yaml");
    print("{}", io::read_text("app.yaml").value_or(string("?")));
    println(config.save("no/such/dir/app.yaml").error().message());
}
```

Output:

```text
name: app
ports:
  - 80
  - 443
input/output error: open no/such/dir/app.yaml: No such file or directory
```

## See also

- [load](load.md)
- [to_string](to_string.md)
- [sgcl::encoding::yaml](README.md)
