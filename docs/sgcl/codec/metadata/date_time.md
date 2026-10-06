[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::date_time

```cpp
optional<time::datetime> date_time(const time::zone& z = time::zone::utc()) const;
```

When the file was last changed: EXIF's DateTime (0x0132) with OffsetTime (0x9010) and SubSecTime (0x9290), else XMP's `xmp:ModifyDate` or `tiff:DateTime`.

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
    println("{}", *camera.date_time());
    println("{}", *edited.date_time());
    println("{}", *camera.date_time(time::zone::fixed(std::chrono::hours(2))));
}
```

Output:

```text
2024-05-06T18:30:00Z
2023-08-02T09:00:00Z
2024-05-06T18:30:00+02:00
```

## See also

- [date_time_original](date_time_original.md): when it was taken
- [time::datetime](../../time/datetime/README.md), [time::zone](../../time/zone/README.md)
- [sgcl::codec::metadata](README.md)
