[sgcl](../../README.md) › [txt](../README.md) › [regex_error](README.md)

# sgcl::txt::regex_error::message

```cpp
string message() const noexcept;
```

Returns the sentence that says why the pattern is not one. From [regex::compile](../regex/compile.md) it is
`sgcl::txt::regex: <the reason> (at byte <n> of the pattern)`, the reason one of those the page of `compile` lists:
a refused construct with why it cannot be had in linear time, a malformed piece, or a limit passed.

## Parameters

None.

## Return value

The sentence; a copy of the string the error holds, which shares its characters.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::regex::compile("a{2,1}").error().message());
    println("{}", txt::regex::compile("(?>a+)b").error().message());
}
```

Output:

```text
sgcl::txt::regex: a count whose bounds are reversed: in '{n,m}' n may not be greater than m (at byte 1 of the pattern)
sgcl::txt::regex: an atomic group: it exists to cut a backtracking engine short, and there is no backtracking here to cut (at byte 0 of the pattern)
```

## See also

- [offset](offset.md): the byte of the pattern
- [sgcl::txt::regex_error](README.md)
