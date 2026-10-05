[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::set_content_id

```cpp
part& set_content_id(const string& id);
```

Content-ID: `<id>` (the id given with or without its brackets) and Content-Disposition: inline (the file's name
kept): a part an HTML shows with `cid:id`. [email::embed](../email/embed.md) does it for a file.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the id, `left@right` |

## Return value

`*this`.

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
    encoding::email::part p("image/png", vector<byte>(4, byte(0)));
    p.set_content_id("logo@example.com");
    println("{} {}", p.header("Content-ID"), p.disposition());
}
```

Output:

```text
<logo@example.com> inline
```

## See also

- [content_id](content_id.md)
- [part](README.md)
