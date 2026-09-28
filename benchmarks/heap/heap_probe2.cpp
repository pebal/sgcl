//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Second round of the audit of 2026-09-26: what a parsed json retains
// against what the parse allocates, csv and xml parses, idna, stencil, the
// prepared searches; and the cases of the changes that followed it (bidi,
// collated text, the typed json, big_integer's formatting). Built and read
// as heap_probe.cpp is.
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
        collector::stepper s(true);
        s.advance_to(collector::stepper::phase::start);
        for (size_t i = 0; i < n; ++i) f();
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

static size_t live_bytes_now() {
    size_t total = 0;
    for (auto& t : collector::get_type_statistics()) {
        total += t.live_bytes;
    }
    return total;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::string doc = "[";
    for (int i = 0; doc.size() < 10000; ++i) {
        if (i) doc += ",";
        doc += "{\"id\":" + std::to_string(i) + ",\"name\":\"item number " + std::to_string(i) + "\",\"tags\":[\"a\",\"b\",\"c\"],\"price\":12.5,\"ok\":true}";
    }
    doc += "]";
    string jdoc(doc);

    // what one parsed document retains, by type
    {
        size_t base = live_bytes_now();
        auto r = encoding::json::parse(jdoc);
        std::cout << "RETAINED json 10KB: " << (live_bytes_now() - base) << " bytes; items " << r->size() << "\n";
        for (auto& t : collector::get_type_statistics()) {
            if (t.live_objects) {
                std::cout << "  " << t.type->name() << (t.buffers ? " [buffers]" : "") << ": " << t.live_objects << " objects, " << t.live_bytes << " bytes\n";
            }
        }
        sink += r->size();
    }

    std::string nums = "[";
    for (int i = 0; nums.size() < 10000; ++i) {
        if (i) nums += ",";
        nums += std::to_string(i * 7);
    }
    nums += "]";
    string jnums(nums);
    measure("json::parse 10KB numbers only", 100, [&] {
        auto r = encoding::json::parse(jnums);
        sink += r->size();
    });
    std::string strs = "[";
    for (int i = 0; strs.size() < 10000; ++i) {
        if (i) strs += ",";
        strs += "\"string value " + std::to_string(i) + "\"";
    }
    strs += "]";
    string jstrs(strs);
    measure("json::parse 10KB strings only", 100, [&] {
        auto r = encoding::json::parse(jstrs);
        sink += r->size();
    });
    measure("json::parse tiny [1,2,3]", 1000, [&] {
        auto r = encoding::json::parse(string("[1,2,3]"));
        sink += r->size();
    });

    // csv
    std::string csvt;
    for (int i = 0; csvt.size() < 10000; ++i) {
        csvt += std::to_string(i) + ",item number " + std::to_string(i) + ",12.5,true,\"quoted, field\"\n";
    }
    string scsv(csvt);
    size_t rows = 0;
    measure("csv reader 10KB (all rows)", 100, [&] {
        encoding::csv::reader r(scsv);
        rows = 0;
        while (auto row = r.next()) {
            ++rows;
            sink += row->size();
        }
    });
    std::cout << "csv rows " << rows << "\n";

    // xml
    std::string xmlt = "<root>";
    for (int i = 0; xmlt.size() < 10000; ++i) {
        xmlt += "<item id=\"" + std::to_string(i) + "\"><name>item number " + std::to_string(i) + "</name><price>12.5</price></item>";
    }
    xmlt += "</root>";
    string sxml(xmlt);
    measure("xml::parse 10KB", 100, [&] {
        auto r = encoding::xml::parse(sxml);
        sink += r ? 1 : 0;
    });

    // idna
    string host("zażółć-gęślą.example.com");
    measure("idna::ascii_form unicode host x1000", 1000, [&] {
        auto o = txt::idna::ascii_form(host);
        sink += o.text.size();
    });
    string ahost("example.com");
    measure("idna::ascii_form ascii host x1000", 1000, [&] {
        auto o = txt::idna::ascii_form(ahost);
        sink += o.text.size();
    });

    // prepared searches
    std::string text;
    while (text.size() < 10000) text += "the quick brown dog jumps over the lazy cat ";
    text += "the quick brown fox jumps over the lazy dog";
    string stext(text);
    txt::folded_text ft(stext);
    txt::fold_searcher fs("FOX");
    measure("folded_text prepared: find x100", 100, [&] {
        auto o = ft.find(fs);
        sink += o ? 1 : 0;
    });
    txt::collator c;
    txt::collated_text ct(c, stext);
    string pat("fox");
    measure("collated_text prepared: find x100", 100, [&] {
        auto o = ct.find(pat);
        sink += o ? 1 : 0;
    });
    txt::searcher bm(string("fox"));
    measure("txt::searcher (plain) find x100", 100, [&] {
        sink += bm.find(stext);
    });

    // stencil
    auto st = *txt::stencil::parse(string("Hello {{ name }}, you have {{ count }} items: {% for i in items %}{{ i }} {% endfor %}"));
    txt::value data = txt::object{{"name", "World"}, {"count", 3}, {"items", txt::list{"a", "b", "c"}}};
    measure("stencil render small x1000", 1000, [&] {
        sink += st.render(data).size();
    });

    // case, identifier
    string mixed("The Quick Brown Fox Jumps Over The Lazy Dog And Zażółć Gęślą Jaźń");
    measure("to_lower_full mixed x1000", 1000, [&] {
        sink += txt::to_lower_full(mixed).size();
    });
    measure("nfkc_casefold mixed x1000", 1000, [&] {
        sink += txt::nfkc_casefold(mixed).size();
    });

    // bidi: the paragraph's arrays, and the answer of each entry point
    std::string bidi_text;
    while (bidi_text.size() < 1024) bidi_text += "abc \xD7\x90\xD7\x91\xD7\x92 123 (x) ";
    string sbidi(bidi_text);
    measure("bidi levels 1KB x100", 100, [&] {
        sink += txt::levels(sbidi).size();
    });
    measure("bidi visual_order 1KB x100", 100, [&] {
        sink += txt::visual_order(sbidi).size();
    });
    measure("bidi_runs 1KB x100", 100, [&] {
        sink += txt::bidi_runs(sbidi).count();
    });

    // the searches of txt that map or weigh the whole text
    measure("fold_searcher::find 10KB x100", 100, [&] {
        auto o = fs.find(stext);
        sink += o ? 1 : 0;
    });
    measure("collator::find 10KB x100", 100, [&] {
        auto o = c.find(stext, pat);
        sink += o ? 1 : 0;
    });
    measure("collated_text build 10KB x100", 100, [&] {
        txt::collated_text t(c, stext);
        sink += t.size();
    });
    measure("folded_text build 10KB x100", 100, [&] {
        txt::folded_text t(stext);
        sink += t.size();
    });

    // normalization and its neighbours
    std::string marks;
    while (marks.size() < 1024) marks += "e\xCC\x81";   // e + U+0301
    string smarks(marks);
    measure("normalize(nfc) 1KB combining x1000", 1000, [&] {
        sink += txt::normalize(smarks, txt::nfc).size();
    });
    measure("without_marks 1KB combining x1000", 1000, [&] {
        sink += txt::without_marks(smarks).size();
    });
    measure("skeleton mixed x1000", 1000, [&] {
        sink += txt::skeleton(mixed).size();
    });

    // regex: the slots of a match with groups
    std::string rtext;
    while (rtext.size() < 1000) rtext += "the quick brown dog jumps over the lazy cat ";
    rtext += "the quick brown fox jumps over the lazy dog";
    string srtext(rtext);
    auto re1 = *txt::regex::compile(string("fox\\s+(\\w+)"));
    auto re4 = *txt::regex::compile(string("(fox)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)"));
    measure("regex find 1 group 1KB x1000", 1000, [&] {
        auto m = re1.find(srtext);
        sink += m ? m->begin_at() : 0;
    });
    measure("regex find 4 groups 1KB x1000", 1000, [&] {
        auto m = re4.find(srtext);
        sink += m ? m->begin_at() : 0;
    });

    // stencil: what a parse of a template keeps and throws away
    string stencil_source("Hello {{.name}}, you have {{.count}} items: {{range .items}}{{.}} {{if .}}!{{else}}?{{end}}{{end}}");
    measure("stencil parse small x1000", 1000, [&] {
        auto p = txt::stencil::parse(stencil_source);
        sink += p ? p->steps() : 0;
    });

    // json: a large object's index, the typed walk into a dynamic_array
    std::string large = "{";
    for (int i = 0; i < 40; ++i) {
        if (i) large += ",";
        large += "\"key" + std::to_string(i) + "\":" + std::to_string(i);
    }
    large += "}";
    string jlarge(large);
    measure("json::parse object of 40 members x1000", 1000, [&] {
        auto r = encoding::json::parse(jlarge);
        sink += r->size();
    });
    std::string ints = "[";
    for (int i = 0; i < 100; ++i) {
        if (i) ints += ",";
        ints += std::to_string(i);
    }
    ints += "]";
    string jints(ints);
    measure("json::parse<dynamic_array<int>> 100 x1000", 1000, [&] {
        auto r = encoding::json::parse<dynamic_array<int>>(jints);
        sink += r->size();
    });

    // big_integer formatted: the digits, and the one string of the result
    std::string hexa;
    for (int i = 0; i < 128; ++i) hexa += "0123456789abcdef"[(i * 7) % 16];
    hexa[0] = 'f';
    auto big = *math::big_integer::parse(string(hexa), 16);
    auto negative = -big;
    measure("format {} big_integer 512 bits x1000", 1000, [&] {
        sink += txt::format("{}", big).size();
    });
    measure("format {:x} -big_integer 512 bits x1000", 1000, [&] {
        sink += txt::format("{:x}", negative).size();
    });

    std::cout << "sink " << sink << "\n";
    return 0;
}
