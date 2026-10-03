[sgcl](../../README.md) › [time](../README.md) › [error](../error.md)

# sgcl::time::error::message

```cpp
string message() const noexcept;
```

The sentence that says why the input is not what it should be: `"a month from 01 to 12 expected"`,
`"unknown time zone \"Europe/Warsw\""`. It is also what `what()` of a `bad_expected_access<time::error>` says, the
exception a constructor of a literal throws.

## Parameters

None.

## Return value

The sentence, as a `string`.

## Complexity

Constant: a copy of the string's word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    println("{}", time::zone::load("Europe/Warsw").error().message());
    try {
        time::date day("2026-02-30");
    } catch (const std::exception& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
unknown time zone "Europe/Warsw"
a day that the month has expected
```

## See also

- [offset](offset.md): where the reading stopped
- [sgcl::time::error](../error.md)
