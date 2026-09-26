# sgcl::encoding::big_endian, sgcl::encoding::little_endian, sgcl::encoding::varint

```cpp
#include "sgcl/encoding/binary.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class big_endian;      // numbers of 16, 32 and 64 bits, the high byte first
    class little_endian;   // the low byte first
    class varint;          // a number in as few bytes as it needs
}
```

The numbers of a binary format, in the order the format gives them: network order, PNG, Java's streams (`big_endian`); the processors of today, ZIP and most formats of the PC (`little_endian`). And the variable-length integer of Go and Protocol Buffers (`varint`): seven bits a byte, the low ones first, the high bit set on every byte but the last — a small number one byte, the largest ten.

## Rules

- `read_u16`, `read_u32`, `read_u64` take the number from the front of the bytes, `write_` puts it there, `append_` adds it at the back of a vector. The bytes of a read or a write hold at least the number's size — a precondition, checked by `assert` as `operator[]` is; a read at an offset is a read of the slice from there (`v.as_slice(4)`). Each is a loop of bytes and shifts that the compiler turns into one load or store and, for the order the processor does not use, one byte swap.
- A signed varint is zigzagged first — 0, -1, 1, -2 to 0, 1, 2, 3 — so that a small negative number is short too (Go's `AppendVarint`).
- `encoding::varint::read` of bytes answers the number and the bytes it took. Bytes that end before the number does are `unexpected_end` at their end; a number past 64 bits — a tenth byte above 1 — is `out_of_range` at that byte. A longer encoding of a number than it needs (`80 00` for 0) is read, as Go reads it.
- `encoding::varint::read` of a [`buffered_reader`](../io/buffered.md) answers `nullopt` at the end of the stream before a number's first byte — a loop over a stream of numbers ends there — `io::errc::unexpected_eof` when the stream ends inside one, and `out_of_range` of the `encoding` category past 64 bits. Go's `ReadUvarint` says `io.EOF`, `io.ErrUnexpectedEOF` and its overflow error for the same three; `co_await async_read(in)` is the same in a task. The reader is taken by reference, since the read moves its position on and a copy would be another reader: one living in a managed object is kept by the task (its frame is traced by the collector), one on the caller's stack has to outlive the task.
- One difference from Go, by name: ten bytes whose tenth has its high bit set are past 64 bits whatever follows, and refused at the tenth. Go's `Uvarint` calls them cut short, and names the eleventh byte when there is one; its `ReadUvarint` refuses at the tenth, as this does.

## Members

```cpp
class big_endian {   // little_endian the same
public:
    static uint16_t read_u16(const slice<const byte>& at) noexcept;
    static uint32_t read_u32(const slice<const byte>& at) noexcept;
    static uint64_t read_u64(const slice<const byte>& at) noexcept;
    static void write_u16(const slice<byte>& at, uint16_t v) noexcept;
    static void write_u32(const slice<byte>& at, uint32_t v) noexcept;
    static void write_u64(const slice<byte>& at, uint64_t v) noexcept;
    static void append_u16(vector<byte>& out, uint16_t v);
    static void append_u32(vector<byte>& out, uint32_t v);
    static void append_u64(vector<byte>& out, uint64_t v);
};

class varint {
public:
    using error = encoding::error;
    static constexpr size_t max_size = 10;

    static void append(vector<byte>& out, uint64_t v);
    static void append_signed(vector<byte>& out, int64_t v);
    static size_t write(const slice<byte>& at, uint64_t v) noexcept;          // the bytes written; max_size always fits
    static size_t write_signed(const slice<byte>& at, int64_t v) noexcept;
    static expected<pair<uint64_t, size_t>, error> read(const slice<const byte>& at);   // the number and the bytes taken
    static expected<pair<int64_t, size_t>, error> read_signed(const slice<const byte>& at);
    static expected<optional<uint64_t>, io::error> read(io::buffered_reader& in);     // nullopt at the end of the stream
    static expected<optional<int64_t>, io::error> read_signed(io::buffered_reader& in);
    static task<expected<optional<uint64_t>, io::error>> async_read(io::buffered_reader& in);
    static task<expected<optional<int64_t>, io::error>> async_read_signed(io::buffered_reader& in);
};
```

## Example

```cpp
#include "sgcl/encoding/binary.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    // a header of eight bytes: a magic number and a size, in network order
    array<byte, 8> header = {};
    encoding::big_endian::write_u32(header, 0xCAFEBABE);
    encoding::big_endian::write_u32(header.as_slice(4), 1234);
    io::stdout.write(encoding::hex::encode(header) + "\n");                     // cafebabe000004d2
    io::stdout.write(string(std::to_string(encoding::big_endian::read_u32(header.as_slice(4)))) + "\n");   // 1234

    // numbers of any size, small ones short
    vector<byte> out;
    encoding::varint::append(out, 300);
    encoding::varint::append_signed(out, -3);
    encoding::little_endian::append_u16(out, 0xABCD);
    io::stdout.write(encoding::hex::encode(out) + "\n");                        // ac0205cdab
    auto [value, size] = encoding::varint::read(out).value();                           // 300, 2
    auto [signed_value, more] = encoding::varint::read_signed(out.as_slice(size)).value();   // -3, 1
    io::stdout.write(string(std::to_string(value) + " " + std::to_string(signed_value) + " " + std::to_string(size + more)) + "\n");   // 300 -3 3
}
```

A stream of numbers read to its end:

```cpp
tracked_ptr in = make_tracked<io::buffered_reader>(io::open("ids.bin").value());
for (;;) {
    auto id = encoding::varint::read(*in);
    if (!id || !*id) {
        break;              // an error (id.error()), or the end
    }
    use(**id);
}
```

## See also

[hex](hex.md) to look at the bytes; [`buffered_reader`](../io/buffered.md); [`error`](error.md).
