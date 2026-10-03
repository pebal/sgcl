[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [writer](README.md)

# sgcl::encoding::json::writer::key

```cpp
writer& key(const string& name) noexcept;                          // (1)
template<size_t N> writer& key(const char (&name)[N]) noexcept;    // (2)
```

The key of the next member of the object open, written with the escapes it needs, as a string is; the member's
value follows it. A key outside an object and a key after a key are mistakes, kept and reported by
[flush](flush.md). The writer does not look for a key written twice in one object: what it is given is what it
writes.

1. A key held in a [string](../../core/string/README.md).
2. A literal or a character array, to its first NUL, written from where it lies with no string made.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the key |

## Return value

`*this`, for the next step in the chain.

## Complexity

Linear in the length of `name`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string unit = "a \"quoted\" unit";
    encoding::json::writer out(io::stdout);
    out.begin_object().key("width").value(3).key(unit).value("cm").end_object();
    out.flush();

    encoding::json::writer twice(io::stdout);
    twice.begin_object().key("a").key("b");
    println(twice.flush().error().message());
}
```

Output:

```text
{"width":3,"a \"quoted\" unit":"cm"}
json: two keys in a row: syntax error
```

## See also

- [value](value.md): the member's value
- [begin_object](begin_object.md): opens an object
- [sgcl::encoding::json::writer](README.md)
