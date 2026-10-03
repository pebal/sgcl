[sgcl](../../README.md) › [encoding](../README.md) › [error](../error.md)

# sgcl::encoding::error::message

```cpp
string message() const noexcept;
```

The error as a sentence for a person: the position — `line:column` when the line is known, `offset N` otherwise —,
the [path](path.md) after it when there is one, a colon, and what went wrong: the detail the format gave
(`invalid character '*'`, `END B does not match BEGIN A`), or the words of the [code](code.md) when it gave none
(`syntax error`, `type mismatch`); the stream's message follows when a stream failed. An error that did not come
from an input text — one `stringify`, `from` or `as` of [json](../json.md) and [xml](../xml.md) gives, a mistake of
the calls to an [xml::writer](../xml-writer.md), or a file of `load` or `save` that does not open, read or write —
has no position: its message is the path and the words, `/x: NaN is not a JSON number`, or the words alone,
`input/output error: open cfg.json: No such file or directory`.

## Parameters

None.

## Return value

The message.

## Complexity

Linear in the length of the detail, the path and the stream's message.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::error e(encoding::errc::type_mismatch, 30);
    println("{}", e.message());
    e.set_path("/users/3/age");
    println("{}", e.message());
    e.set_position(3, 14);
    println("{}", e.message());
    encoding::error own(encoding::errc::syntax, 7, "unexpected ']'");
    println("{}", own.set_position(1, 8).message());
}
```

Output:

```text
offset 30: type mismatch
offset 30 /users/3/age: type mismatch
3:14 /users/3/age: type mismatch
1:8: unexpected ']'
```

## See also

- [errc](../errc.md): the words of each code
- [sgcl::encoding::error](../error.md)
