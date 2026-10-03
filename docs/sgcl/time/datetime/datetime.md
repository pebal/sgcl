[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::datetime

```cpp
datetime() noexcept = default;                                            // (1)
explicit datetime(std::chrono::sys_time<std::chrono::nanoseconds> t,      // (2)
                  const time::zone& z = time::zone::local()) noexcept;
explicit datetime(const string& text, layout format);                     // (3)
explicit datetime(const string& text, const string& pattern,              // (4)
                  const time::zone& z = time::zone::utc());
```

Constructs a datetime.

1. 1970-01-01T00:00:00Z, in UTC.
2. The instant `t` of the system clock, in the zone `z`, the local zone unless another is given, as in Go. It is what
   `std::chrono::system_clock::now()` and [io::file_info::modified](../../io/file_info.md) are, of the same range and unit,
   taken as they are; a `sys_time` of a coarser unit (`sys_seconds`, `sys_days`) converts to it.
3. The datetime a literal in the program spells in a [layout](../layout.md):
   `time::datetime t("2026-09-24T12:41:15+02:00", time::rfc3339)`. What [parse](parse.md) reads, or
   `bad_expected_access<time::error>` with `parse`'s error.
4. The same for a [pattern](../README.md#patterns) of `%`, read in the zone `z` (UTC unless another is given)
   where the text has no offset.

A text the program itself writes is constructed; a text from outside it (input, a file, the network) is parsed,
and its error is a value.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the instant, the nanoseconds since 1970 |
| `z` | the zone the datetime is seen in; for (4), the zone of a time without an offset |
| `text` | the text of the datetime |
| `format` | the layout of `text`: `time::rfc3339`, `rfc3339_nano`, `http`, `email` or `iso8601` |
| `pattern` | the pattern of `text`, `%` specifiers and the characters between them |

## Complexity

- (1–2) Constant.
- (3–4) Linear in the length of `text` (and of `pattern`), plus the look-up of the time in the zone's changes.

## Exceptions

- (1–2) None.
- (3–4) `bad_expected_access<time::error>` when `text` is not a datetime of the layout or the pattern; its
  `error()` is the [error](../error.md) of `parse`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <chrono>

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::datetime epoch;
    auto sys = std::chrono::sys_days(std::chrono::year(2026) / 9 / 24) + std::chrono::hours(10);
    time::datetime t(sys, warsaw);
    println("{} {}", epoch, t);

    time::datetime spelled("2026-09-24T12:41:15+02:00", time::rfc3339);
    time::datetime wall("24.09.2026 12:41", "%d.%m.%Y %H:%M", warsaw);
    println("{} {}", spelled, wall);

    try {
        time::datetime wrong("2026-09-31T12:00:00Z", time::rfc3339);
    } catch (const bad_expected_access<time::error>& e) {
        println("{} (byte {})", e.error().message(), e.error().offset());
    }
}
```

Output:

```text
1970-01-01T00:00:00Z 2026-09-24T12:00:00+02:00
2026-09-24T12:41:15+02:00 2026-09-24T12:41:00+02:00
a day that the month has expected (byte 0)
```

## See also

- [parse](parse.md): reads a text from outside the program
- [from_unix](from_unix.md): a datetime of the seconds since 1970
- [to_sys](to_sys.md): the instant back as a `std::chrono::sys_time`
- [sgcl::time::datetime](../datetime.md)
