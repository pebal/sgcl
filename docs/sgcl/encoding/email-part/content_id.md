[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::content_id

```cpp
string content_id() const;
```

The Content-ID without its angle brackets: what an HTML refers to with `cid:`.

## Parameters

None.

## Return value

The id; `""` when there is none.

## Complexity

Linear in the size of the head.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::email::part p("image/png", vector<byte>(4, byte(0)));
    p.set_content_id("<logo@example.com>");
    println("{}", p.content_id());
}
```

Output:

```text
logo@example.com
```

## See also

- [set_content_id](set_content_id.md)
- [part](README.md)
