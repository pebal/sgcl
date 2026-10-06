[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::to_string

```cpp
string to_string() const;
```

The entries as a `.env` text, one `KEY=value` a line in their order: a value plain when it reads back as itself
(no blank at its ends, none of `"`, `'`, `#`, `$`, `\\` and no control character), else in double quotes with
`\\`, `\"`, `\$`, `\n`, `\r` and `\t`, so that [parse](parse.md) reads it back with expansion on.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the entries.

## Exceptions

`invalid_argument` for a key a `.env` cannot hold (one made by [set](set.md) or [from](from.md)), or a value with a null character.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv env = encoding::dotenv::from({{"PLAIN", "value"}, {"SPACED", "two words"}, {"PRICE", "$5"},
                                                   {"LINES", "one\ntwo"}, {"EMPTY", ""}});
    print(env.to_string());
}
```

Output:

```text
PLAIN=value
SPACED=two words
PRICE="\$5"
LINES="one\ntwo"
EMPTY=
```

## See also

- [parse](parse.md)
- [save](save.md)
- [sgcl::encoding::dotenv](README.md)
