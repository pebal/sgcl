//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Managed allocations per operation, read off the collector's cycle log
// (built with SGCL_LOG_PRINT_LEVEL=2, see ../CMakeLists.txt): "objects
// created" of the cycles between the BEGIN and END markers, and the pages of
// 64 KB taken while the collector was parked. The audit of 2026-09-26 took
// its numbers with this probe; parse.py turns the output into one line per
// case: bench_heap_probe > out.txt && python3 parse.py out.txt
#include "sgcl/sgcl.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

using namespace sgcl;

static volatile size_t sink = 0;

template<class F>
void measure(const char* name, size_t n, F&& f) {
    using sgcl::detail::MemoryCounters;
    collector::force_collect();
    collector::force_collect();
    size_t p0, p1, p2;
    {
        collector::stepper s(true);          // the collector parked: no sweep, no slot reuse
        s.advance_to(collector::stepper::phase::start);
        for (size_t i = 0; i < n; ++i) f();  // warm-up: fills the free slots of earlier sweeps
        p0 = MemoryCounters::alloc_since_cycle();
        for (size_t i = 0; i < n; ++i) f();
        p1 = MemoryCounters::alloc_since_cycle();
        for (size_t i = 0; i < n; ++i) f();
        p2 = MemoryCounters::alloc_since_cycle();
        std::cout << "BEGIN " << name << "\n" << std::flush;
        s.finish_cycle();
        s.finish_cycle();
        s.finish_cycle();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "END " << name << " n=" << n << " pages=" << (p1 - p0) << "," << (p2 - p1) << "\n" << std::flush;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    // --- calibration: what "objects created" counts ---
    measure("calib make_tracked<int>", 1000, [] {
        auto p = make_tracked<int>(1);
        sink += size_t(*p);
    });
    measure("calib vector<int>(100)", 1000, [] {
        vector<int> v(100);
        sink += v.size();
    });
    measure("calib vector<char32_t> reserve(1024)", 1000, [] {
        vector<char32_t> v;
        v.reserve(1024);
        sink += v.capacity();
    });
    measure("calib string 20 bytes", 1000, [] {
        string s("abcdefghijklmnopqrst");
        sink += s.size();
    });
    measure("calib nothing", 1000, [] {
        sink += 1;
    });

    // --- (1) json::parse of a 10 KB document ---
    std::string doc = "[";
    for (int i = 0; doc.size() < 10000; ++i) {
        if (i) doc += ",";
        doc += "{\"id\":" + std::to_string(i) + ",\"name\":\"item number " + std::to_string(i) + "\",\"tags\":[\"a\",\"b\",\"c\"],\"price\":12.5,\"ok\":true}";
    }
    doc += "]";
    string jdoc(doc);
    std::cout << "json doc bytes " << jdoc.size() << "\n";
    measure("json::parse 10KB", 100, [&] {
        auto r = encoding::json::parse(jdoc);
        sink += r ? r->size() : 0;
    });

    // --- (2) collator ---
    txt::collator c;
    string a("abcdefghij0123456789"), b("abcdefghij0123456788");
    string ua("Zażółć gęślą jaźń"), ub("Zażółć gęślą jaźn");
    measure("collator::compare ascii x1000", 1000, [&] {
        sink += size_t(c.compare(a, b));
    });
    measure("collator::compare polish x1000", 1000, [&] {
        sink += size_t(c.compare(ua, ub));
    });
    {
        byte buf[256];
        measure("collator::key_to buffer x1000", 1000, [&] {
            sink += c.key_to(slice<byte>(buf, buf + 256), ua);
        });
        measure("collator::key vector x1000", 1000, [&] {
            sink += c.key(ua).size();
        });
    }

    // --- (3) normalize ---
    std::string ascii(1024, 'x');
    string sascii(ascii);
    std::string marks;
    while (marks.size() < 1024) marks += "e\xCC\x81";   // e + U+0301
    string smarks(marks);
    measure("normalize(nfc) 1KB ascii", 1000, [&] {
        sink += txt::normalize(sascii, txt::nfc).size();
    });
    measure("normalize(nfc) 1KB combining", 1000, [&] {
        sink += txt::normalize(smarks, txt::nfc).size();
    });
    measure("normalize(nfd) 1KB combining (already nfd)", 1000, [&] {
        sink += txt::normalize(smarks, txt::nfd).size();
    });

    // --- (4) big_integer 2048-bit multiply ---
    std::string hexa, hexb;
    for (int i = 0; i < 512; ++i) { hexa += "0123456789abcdef"[(i * 7) % 16]; hexb += "0123456789abcdef"[(i * 11 + 3) % 16]; }
    hexa[0] = 'f'; hexb[0] = 'e';
    auto ba = *math::big_integer::parse(string(hexa), 16);
    auto bb = *math::big_integer::parse(string(hexb), 16);
    std::cout << "bits " << ba.bit_length() << " " << bb.bit_length() << "\n";
    measure("big_integer 2048x2048 multiply x1000", 1000, [&] {
        auto p = ba * bb;
        sink += p.bit_length();
    });
    measure("big_integer 2048 to_string(16) x1000", 1000, [&] {
        sink += ba.to_string(16).size();
    });

    // --- (5) format ---
    measure("format {} {} {} x1000", 1000, [&] {
        sink += txt::format("{} {} {}", 1, 2, 3).size();
    });

    // --- (6) regex ---
    std::string text;
    while (text.size() < 10000) text += "the quick brown dog jumps over the lazy cat ";
    text += "the quick brown fox jumps over the lazy dog";
    string stext(text);
    auto re = *txt::regex::compile(string("fox\\s+(\\w+)"));
    auto re4 = *txt::regex::compile(string("(fox)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)"));
    auto re0 = *txt::regex::compile(string("fox\\s+jumps"));
    measure("regex find 1 group 10KB x100", 100, [&] {
        auto m = re.find(stext);
        sink += m ? m->begin_at() : 0;
    });
    measure("regex find 4 groups 10KB x100", 100, [&] {
        auto m = re4.find(stext);
        sink += m ? m->begin_at() : 0;
    });
    measure("regex find 0 groups 10KB x100", 100, [&] {
        auto m = re0.find(stext);
        sink += m ? m->begin_at() : 0;
    });
    measure("regex contains 10KB x100", 100, [&] {
        sink += re0.contains(stext);
    });

    // --- extras ---
    txt::fold_searcher fs("FOX");
    measure("fold_searcher::find 10KB x100", 100, [&] {
        auto o = fs.find(stext);
        sink += o ? 1 : 0;
    });
    string pat("fox");
    measure("collator::find 10KB x100", 100, [&] {
        auto o = c.find(stext, pat);
        sink += o ? 1 : 0;
    });
    std::string bidi_text;
    while (bidi_text.size() < 1024) bidi_text += "abc \xD7\x90\xD7\x91\xD7\x92 123 (x) ";
    string sbidi(bidi_text);
    measure("bidi levels 1KB x100", 100, [&] {
        sink += txt::levels(sbidi).size();
    });
    measure("to_utf16 1KB x1000", 1000, [&] {
        sink += txt::to_utf16(sascii).size();
    });
    std::cout << "sink " << sink << "\n";
    return 0;
}
