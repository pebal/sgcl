[sgcl](../../README.md) › [time](../README.md)

# sgcl::time::error

```cpp
#include "sgcl/time/error.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class error;
}
```

`sgcl::time::error` is why a text is not a date or a time, or a file not a time zone: a sentence and the byte of the
text or of the file the reading stopped on. It is the one type of error of the whole module.

A text that may not be what it should (a date from a person, from a file, from the network) and a zone that may
not be there are read into an [expected](../../core/expected/README.md)`<T, time::error>`, the value or the error:
[datetime::parse](../datetime/parse.md), [date::parse](../date/parse.md), [zone::load](../zone/load.md),
[zone::from_tzif](../zone/from_tzif.md) and [zone::from_posix](../zone/from_posix.md) return one. A text or a name the
program itself writes is constructed instead — `time::zone warsaw("Europe/Warsaw")`, a
[datetime](../datetime/datetime.md) or a [date](../date/date.md) of a literal text — and a wrong one throws a
[bad_expected_access](../../core/bad_expected_access/README.md)`<time::error>` that carries the error the reading returned,
its `what()` the error's [message](message.md). What else in the module throws, and what does not, is in
[the rules](../README.md#the-rules) of the module.

## Rules

- An error is two words, the sentence (a [string](../../core/string/README.md)) and the offset: it lives where a string
  may.
- The sentences are the library's, one per refusal: `"a month from 01 to 12 expected"`,
  `"unknown time zone \"Europe/Warsw\""`, `"TZif: the header is cut short"`. The offset is the byte where the
  field that failed starts, in the text or in the bytes of the file; 0 where the whole input is refused, a name
  of a time zone the system does not have.
- Two errors are equal when they say the same sentence at the same byte.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](error.md) | constructs an error of a sentence and an offset |
| [message](message.md) | the sentence |
| [offset](offset.md) | the byte of the input the reading stopped on |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same sentence at the same byte |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string setting = "2026-13-01";
    auto day = time::date::parse(setting);
    if (!day) {
        const time::error& e = day.error();
        println("\"{}\": {} at byte {}", setting, e.message(), e.offset());
    }
}
```

Output:

```text
"2026-13-01": a month from 01 to 12 expected at byte 5
```

## See also

- [datetime::parse](../datetime/parse.md), [date::parse](../date/parse.md), [zone::load](../zone/load.md): what returns
  it
- [expected](../../core/expected/README.md): the value or the error
- [sgcl::time](../README.md)
