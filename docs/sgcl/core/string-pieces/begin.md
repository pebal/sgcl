[sgcl](../../README.md) › [core](../README.md) › [string](../string/README.md) › [pieces](README.md)

# sgcl::string::pieces::begin

```cpp
iterator begin() const noexcept;
```

Returns an iterator to the first piece. Every call walks anew from the start of the string: the first piece is found
by the call, the next ones as the iterator is advanced, one search each. When the string is empty, or has no piece
(a `fields` of white space alone), the iterator is `end()`.

The iterator holds a pointer to this range and is valid while the range lives: a range-for over `s.split(',')` keeps
the temporary range for the loop, while an iterator kept from `s.split(',').begin()` dangles. The slice it refers to,
`*it`, holds the string and stays valid on its own when copied out.

## Parameters

None.

## Return value

An iterator to the first piece; `end()` when there is no piece.

## Complexity

Linear in the length of the string up to the end of the first piece: one search.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string path = "usr/local/bin";
    string::pieces dirs = path.split('/');

    auto it = dirs.begin();
    println("{}", *it);
    ++it;
    println("{} {}", *it, it->size());

    println("{}", *dirs.begin());  // a new walk, from the start

    string::pieces none = string().split('/');
    println("{}", none.begin() == none.end());
}
```

Output:

```text
usr
local 5
usr
true
```

## See also

- [end](end.md): the iterator past the last piece
- [empty](empty.md): checks whether there is no piece
- [sgcl::string::pieces](README.md)
