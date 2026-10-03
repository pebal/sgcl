[sgcl](../../README.md) › [core](../README.md) › [duration_error](README.md)

# sgcl::duration_error::message

```cpp
string message() const noexcept;
```

The sentence that says why the text is not a duration: `"an unknown unit: ns, us, ms, s, m or h expected"`. It is
also what `what()` of a `bad_expected_access<duration_error>` says.

## Parameters

None.

## Return value

The sentence, as a `string`.

## Complexity

Linear in the length of the sentence.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", duration::parse("1x").error().message());
    try {
        duration d("1.5");
    } catch (const std::exception& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
an unknown unit: ns, us, ms, s, m or h expected
a unit expected: ns, us, ms, s, m or h
```

## See also

- [offset](offset.md): where the reading stopped
- [sgcl::duration_error](README.md)
