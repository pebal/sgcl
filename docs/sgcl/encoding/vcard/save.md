[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::save, async_save

```cpp
expected<void, error> save(const string& path) const;                         // (1)
async::task<expected<void, error>> async_save(string path) const noexcept;    // (2)
```

The card's [to_string](to_string.md) into a file, made or written over.

1. Written now.
2. (1) for a task: the card copied into it, the file written on a thread of the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

Nothing, or the [error](../error/README.md) `io`, without a place, for a file that does not open or write.

## Complexity

Linear in the size of the card.

## Exceptions

- (1) What [to_string](to_string.md) throws, and what a write of the file throws.
- (2) None from the call; awaiting the task throws what (1) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::vcard().add(encoding::content_line::text("FN", "Ann")).save("ann.vcf");
    print("{}", io::read_text("ann.vcf").value_or(string("?")));
    println(encoding::vcard().save("no/such/dir/a.vcf").error().message());
}
```

Output:

```text
BEGIN:VCARD
VERSION:4.0
FN:Ann
END:VCARD
input/output error: open no/such/dir/a.vcf: No such file or directory
```

## See also

- [load](load.md)
- [sgcl::encoding::vcard](README.md)
