[sgcl](../README.md) › [time](README.md)

# sgcl::time::earlier_t, sgcl::time::later_t

```cpp
#include "sgcl/time/date.h"   // or "sgcl/time.h"

namespace sgcl::time {
    inline constexpr struct earlier_t {
        explicit earlier_t() = default;
    } earlier{};

    inline constexpr struct later_t {
        explicit later_t() = default;
    } later{};
}
```

The tags that choose how a time of the clock that a change of the clock makes ambiguous is read by a date's
[at](date/at.md). A time shown twice (the hour repeated in autumn) is two instants: `earlier` takes the first,
`later` the second. A time skipped (the hour lost in spring) is none: `earlier` takes the instant of the change,
the first time of the clock after the skip (03:00 for 02:30 on the night Warsaw goes from 02:00 to 03:00), and
`later` moves it on by the length of the skip (03:30). Without a tag `at` reads by the compatible rule, the first
of a time shown twice and a skipped time moved on, which is what Java, JavaScript's Temporal and iCalendar
(RFC 5545) do; [try_at](date/try_at.md) is nothing for both.

## Rules

- Empty types, passed by value. The default constructor is explicit, so a tag is not made from a bare `{}`
  (`d.at(2, 30, z, {})` does not compile): it is written by its constant, `time::earlier` or `time::later`.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the default constructor, explicit |

## Non-member functions

#### Constants

| Constant | Value | Description |
|---|---|---|
| `earlier` | `earlier_t{}` | the first of a time shown twice, and the change for a time skipped, `inline constexpr earlier_t` |
| `later` | `later_t{}` | the second of a time shown twice, and a time skipped moved on, `inline constexpr later_t` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <type_traits>

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::date autumn(2026, 10, 25);
    println(autumn.at(2, 30, warsaw, time::earlier));
    println(autumn.at(2, 30, warsaw, time::later));
    println("{}", std::is_empty_v<time::earlier_t>);
}
```

Output:

```text
2026-10-25T02:30:00+02:00
2026-10-25T02:30:00+01:00
true
```

## See also

- [at](date/at.md): the times of the clock the tags read
- [try_at](date/try_at.md): nothing for a time skipped or shown twice
- [time](README.md)
