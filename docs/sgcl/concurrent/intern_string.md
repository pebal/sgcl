[sgcl](../README.md) › [concurrent](README.md)

# sgcl::concurrent::intern_string

```cpp
#include "sgcl/concurrent/intern.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    string intern_string(std::string_view s);
}
```

Returns the string of the characters of `s`, interned in the default pool of strings: the one string every thread
holds for them. It is `intern<string>::make(s)` under a name that reads ([intern](intern/README.md)). A `string_view`, a
literal or a slice of another string is looked up as it is: no string is made when the value is known, and one is
made, and entered, when it is new.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the characters to intern |

## Return value

The interned string: equal strings interned anywhere in the program are the same object, compared in one word by
`object()`. The empty string, which is null, for an empty `s`.

## Complexity

Constant on average, as [of](intern/of.md): a search of the default pool's hash set, and for a new value the string
made and the insertion.

## Exceptions

`length_error` when `s` is longer than `string::max_size()`, 4 GiB less one: no string of its characters can be
made. The pool is then as it was.

## Notes

Lock-free: two threads interning the same new characters at once both get the string of the one whose entry won.
The pool holds its strings weakly: an interned string nobody holds any more is collected, and the next
`intern_string` of its characters makes a new one. Interning pays where a value repeats across many records and is
compared or hashed often: one copy of each, and a comparison of two by identity.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string a = concurrent::intern_string("alpha");
    string line = "alpha 512";
    string b = concurrent::intern_string(line.as_slice(0, 5));  // a slice of another string
    println("{} {}", a, a.object() == b.object());

    string empty = concurrent::intern_string("");
    println("{}", empty.object() == nullptr);
}
```

Output:

```text
alpha true
true
```

## See also

- [intern](intern/README.md): the pool, for strings and any other type
- [make](intern/make.md): the default pool of any type
- [string](../core/string/README.md): one word, compared by identity first
