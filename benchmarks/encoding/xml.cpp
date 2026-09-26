//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// XML of the encoding module against Go's encoding/xml (benchmarks/go/xml,
// the same document, the same work).
//   xml sgcl [op=tokens] [books=5000] [count]
//
//   tokens   every token of the document in memory, xml::reader::next
//            (Go: Decoder.Token over a bytes.Reader)
//   stream   the same through a stream handing out 4 KB a read
//            (Go: Decoder.Token over a reader of 4 KB reads)
//   tree     xml::parse, the whole tree (Go has no tree: Unmarshal into a
//            node struct of any elements, attributes and chardata)
//   write    to_string of the tree (Go: Marshal of the same nodes)
//   typed    xml::parse<catalog>, the books into a program's structures
//            through describe (Go: Unmarshal into tagged structs)
//
// The document: a catalog of `books` elements, each with attributes, a
// namespace prefix, text with references and a nested element; the same
// bytes on both sides. A run takes about two seconds by default. Prints
// nanoseconds per document and megabytes of it per second.
#include "benchmarks/common.h"
#include "sgcl/async/scheduler.h"
#include "sgcl/encoding/xml.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    std::string document(int books) {
        std::string d = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<catalog xmlns=\"urn:books\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n";
        for (int i = 0; i < books; ++i) {
            auto n = std::to_string(i);
            d += "  <book id=\"b" + n + "\" lang=\"pl\" available=\"true\">\n";
            d += "    <dc:title>Title number " + n + " &amp; more</dc:title>\n";
            d += "    <author first=\"Jan\" last=\"Kowalski\"/>\n";
            d += "    <price currency=\"PLN\">" + std::to_string(10 + i % 90) + ".99</price>\n";
            d += "    <description>A book about &lt;things&gt;, number " + n + ", zażółć gęślą jaźń.</description>\n";
            d += "  </book>\n";
        }
        return d + "</catalog>\n";
    }

    // A stream handing out at most `piece` bytes a read
    class pieces final : public sgcl::io::mixin::reader<pieces> {
    public:
        pieces(const std::string& s, size_t piece) : _s(s), _piece(piece) {}

        sgcl::expected<size_t, sgcl::io::error> read(sgcl::slice<sgcl::byte> out) {
            size_t k = std::min({out.size(), _piece, _s.size() - _at});
            std::memcpy(out.data(), _s.data() + _at, k);
            _at += k;
            return k;
        }

        sgcl::async::task<sgcl::expected<size_t, sgcl::io::error>> async_read(sgcl::slice<sgcl::byte> out) {
            co_return read(out);
        }

    private:
        const std::string& _s;
        size_t _piece;
        size_t _at = 0;
    };

    struct author {
        sgcl::string first, last;
        void describe(sgcl::encoding::field_list& f) {
            f.add("first", first).attribute();
            f.add("last", last).attribute();
        }
    };

    struct price {
        sgcl::string currency;
        double amount = 0;
        void describe(sgcl::encoding::field_list& f) {
            f.add("currency", currency).attribute();
            f.add("amount", amount).text();
        }
    };

    struct book {
        sgcl::string id, lang, title, description;
        bool available = false;
        author by;
        price cost;
        void describe(sgcl::encoding::field_list& f) {
            f.add("id", id).attribute();
            f.add("lang", lang).attribute();
            f.add("available", available).attribute();
            f.add("dc:title", title);
            f.add("author", by);
            f.add("price", cost);
            f.add("description", description);
        }
    };

    struct catalog {
        sgcl::vector<book> books;
        void describe(sgcl::encoding::field_list& f) {
            f.add("book", books);
        }
    };

    volatile size_t sink = 0;

    template<class F>
    double timed(long count, F&& f) {
        for (long i = 0; i < std::min(count, 3L); ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            f();
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }

    double run_sgcl(const char* op, const std::string& doc, long count) {
        using namespace sgcl;
        using encoding::xml;
        string text(doc);
        if (!std::strcmp(op, "tokens")) {
            return timed(count, [&] {
                xml::reader r(text);
                size_t n = 0;
                while (r.next()) {
                    ++n;
                }
                sink += n;
            });
        }
        if (!std::strcmp(op, "stream")) {
            return timed(count, [&] {
                xml::reader r(make_tracked<pieces>(doc, 4096));
                size_t n = 0;
                while (r.next()) {
                    ++n;
                }
                sink += n;
            });
        }
        if (!std::strcmp(op, "typed")) {
            return timed(count, [&] { sink += xml::parse<catalog>(text)->books.size(); });
        }
        if (!std::strcmp(op, "tree")) {
            return timed(count, [&] { sink += xml::parse(text)->children().size(); });
        }
        auto tree = xml::parse(text).value();
        return timed(count, [&] { sink += tree.to_string().size(); });
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl"})) {
        std::fprintf(stderr, "usage: xml sgcl [tokens|stream|tree|write|typed] [books] [count]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "tokens";
    if (!bench::has_variant(op, {"tokens", "stream", "tree", "write", "typed"})) {
        std::fprintf(stderr, "xml: no op called %s\n", op);
        return 2;
    }
    int books = argc > 3 ? std::atoi(argv[3]) : 5000;
    auto doc = document(books);
    long count = argc > 4 ? std::atol(argv[4]) : long(2'000'000'000 / 5 / (doc.size() ? doc.size() : 1)) + 1;
    double ns = run_sgcl(op, doc, count);
    std::printf("%s op=%s books=%d bytes=%zu count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, books, doc.size(), count, ns,
                double(doc.size()) / ns * 1e3);
    return 0;
}
