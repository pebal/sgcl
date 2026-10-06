[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::date_time_original

```cpp
optional<time::datetime> date_time_original(const time::zone& z = time::zone::utc()) const;
```

When the picture was taken: EXIF's DateTimeOriginal (0x9003) with OffsetTimeOriginal (0x9011) and SubSecTimeOriginal (0x9291), else XMP's `exif:DateTimeOriginal` or `photoshop:DateCreated`.

The time is in a fixed zone of the offset the file gives (EXIF 2.31's offset fields, or the offset of XMP's ISO 8601
text). A file of an older camera gives none: its time is the clock of the place it was taken, which the file does not
name, and it is read as a time of `z`, UTC unless another zone is given, as
[datetime::parse](../../time/datetime/parse.md) reads a text without an offset. A time of all zeros or blanks (EXIF's
"unknown"), and a date the calendar does not have, is `nullopt`; a leap second, `:60`, is read as `:59`.

## Parameters

| Parameter | Description |
|---|---|
| `z` | the zone of a time the file gives without an offset; the default is UTC |

## Return value

The time, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    codec::metadata camera = codec::metadata::load("tests/codec/fuzz/seeds/metadata/camera.jpg");
    codec::metadata edited = codec::metadata::load("tests/codec/fuzz/seeds/metadata/xmp.webp");
    println("{}", *camera.date_time_original());
    println("{}", *edited.date_time_original());
    // the file's offset wins
    println("{}", *camera.date_time_original(time::zone::fixed(std::chrono::hours(-5))));
}
```

Output:

```text
2024-05-06T18:29:41.25+02:00
2023-08-01T12:15:30+02:00
2024-05-06T18:29:41.25+02:00
```

## See also

- [date_time_digitized](date_time_digitized.md): when it was stored
- [time::datetime](../../time/datetime/README.md), [time::zone](../../time/zone/README.md)
- [sgcl::codec::metadata](README.md)
