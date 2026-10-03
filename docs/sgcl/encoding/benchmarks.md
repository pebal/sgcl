[sgcl](../README.md) › [encoding](README.md)

# Benchmarks: encoding

The setup, the machine, the environments and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md). The module's cases are in
`benchmarks/encoding/`, three programs, each with its Go counterpart over the same bytes and the same work: one
case a process, run for about two seconds, printing the nanoseconds per call and the megabytes of input per
second. `benchmarks/compare.sh` runs JSON, CSV and XML side by side (`CASES="json xml"`).

## The byte codecs

`bench_encoding` (`benchmarks/encoding/encoding.cpp`) against `benchmarks/go/encoding`, as
`encoding sgcl [op] [size=1024] [count]`: the input is `size` bytes from a generator (splitmix64, seeded by the
size), the same on both sides, and the text decoded is its encoding; the count is 2 GB over the size by default.

| Case | What is measured |
|---|---|
| `b32enc`, `b32dec` | `base32::standard`'s `encode` and `decode`; Go's `base32.StdEncoding` |
| `b64dec` | `base64::standard.decode`, a vector made each time; Go's `StdEncoding.DecodeString` |
| `b64decto` | `decode_to` into a buffer the caller keeps; Go's `Decode` |
| `b64enc` | `base64::standard.encode`, a string made each time; Go's `StdEncoding.EncodeToString` |
| `b64to` | `encode_to` into a buffer the caller keeps; Go's `Encode` |
| `hexenc`, `hexdec` | `hex::encode` and `hex::decode`; Go's `hex.EncodeToString` and `hex.DecodeString` |

## JSON and CSV

`bench_json` (`benchmarks/encoding/json.cpp`) against `benchmarks/go/json` (`encoding/json/v2`, `jsontext`,
`encoding/csv`), as `json sgcl [op] [corpus=twitter] [seconds=2]`. A corpus of nativejson-benchmark is read from
`~/Programming/oracles/nativejson/<corpus>.json` (`twitter`, `citm_catalog`, `canada`); `strings` is made by both
sides: 2000 long ASCII strings, an escape in each. For the typed and the CSV cases the corpus is the count of
records (10000 in `compare.sh`).

| Case | What is measured |
|---|---|
| `csv` | `csv::reader::next` over the rows; Go's `csv.Reader`, `ReuseRecord` off |
| `csvtyped` | `csv::reader::read<T>` of the same rows; Go's `csv.Reader` and `strconv` per field |
| `csvwrite` | `csv::writer` of the same rows; Go's `csv.Writer` |
| `parse` | `json::parse` of the text, a tree made each time; Go's v2 `Unmarshal` into `any` (and `parse1`, v1, on Go's side) |
| `pretty` | `to_string(json::pretty)`; Go's `Marshal` with an indent of two |
| `skip` | `json::reader::skip`: the text checked; Go's `jsontext.Value.IsValid` |
| `stringify` | `json::stringify` of the records; Go's `Marshal` |
| `tokens` | `json::reader::next` to the end; Go's `jsontext.Decoder.ReadToken` |
| `typed` | `json::parse<records>`; Go's `Unmarshal` into a slice of structs |
| `write` | `to_string` of the tree parsed once; Go's v2 `Marshal` of the `any` |

## XML

`bench_xml` (`benchmarks/encoding/xml.cpp`) against `benchmarks/go/xml` (`encoding/xml`), as
`xml sgcl [op] [books=5000] [count]`. The document is a catalog of `books` elements, each with attributes, a
namespace prefix, text with references and a nested element, the same bytes on both sides (1.47 MB for 5000
books); the time is per document.

| Case | What is measured |
|---|---|
| `stream` | the same through a stream handing out 4 KB a read; Go's `Decoder.Token` over a reader of 4 KB reads |
| `tokens` | every token of the document in memory, `xml::reader::next`; Go's `Decoder.Token` over a `bytes.Reader` |
| `tree` | `xml::parse`, the whole tree; Go has no tree: `Unmarshal` into a node struct of any elements, attributes and chardata |
| `typed` | `xml::parse<catalog>`, the books into a program's structures through `describe`; Go's `Unmarshal` into tagged structs |
| `write` | `to_string` of the tree; Go's `Marshal` of the same nodes |

## See also

- [sgcl::encoding](README.md)
- [Benchmarks: hash](../hash/benchmarks.md)
