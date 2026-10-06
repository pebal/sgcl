// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of sgcl/txt/html_stencil.h: Go's html/template asked the same
// questions. Only a tool, never in the library; tools/html_stencil_vectors.py
// runs it (go run tools/html_stencil_oracle.go) over the cases it writes.
//
// A question is a line: the template (with the dot, which both sides read
// alike) TAB the data, name=type:value separated by ';' (i an integer, d a
// double, s a text, b a truth, n nothing), each field with \\ \t \n \; \=
// escaped. The data also always holds "list" (["a", "<b>", "c\"d"]) and
// "obj" ({"k": "v<"}). The answer is the page, \\ \t \n escaped, or ERROR.
// The library's safe_html, safe_url, safe_attr, safe_js and safe_css are
// Go's typed strings HTML, URL, HTMLAttr, JS and CSS.
package main

import (
	"bufio"
	"fmt"
	"html/template"
	"os"
	"strconv"
	"strings"
)

func unescape(s string) string {
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		if s[i] == '\\' && i+1 < len(s) {
			i++
			switch s[i] {
			case 't':
				b.WriteByte('\t')
			case 'n':
				b.WriteByte('\n')
			case '0':
				b.WriteByte(0)
			default:
				b.WriteByte(s[i])
			}
		} else {
			b.WriteByte(s[i])
		}
	}
	return b.String()
}

func split(s string, sep byte) []string {
	out := []string{""}
	for i := 0; i < len(s); i++ {
		if s[i] == '\\' && i+1 < len(s) {
			out[len(out)-1] += s[i : i+2]
			i++
		} else if s[i] == sep {
			out = append(out, "")
		} else {
			out[len(out)-1] += s[i : i+1]
		}
	}
	return out
}

// the library's safe_ functions as Go's typed strings
var funcs = template.FuncMap{
	"safe_html": func(v any) template.HTML { return template.HTML(fmt.Sprint(v)) },
	"safe_url":  func(v any) template.URL { return template.URL(fmt.Sprint(v)) },
	"safe_attr": func(v any) template.HTMLAttr { return template.HTMLAttr(fmt.Sprint(v)) },
	"safe_js":   func(v any) template.JS { return template.JS(fmt.Sprint(v)) },
	"safe_css":  func(v any) template.CSS { return template.CSS(fmt.Sprint(v)) },
}

func main() {
	in := bufio.NewScanner(os.Stdin)
	in.Buffer(make([]byte, 1<<20), 1<<24)
	w := bufio.NewWriter(os.Stdout)
	defer w.Flush()
	for in.Scan() {
		f := split(in.Text(), '\t')
		for len(f) < 2 {
			f = append(f, "")
		}
		data := map[string]any{"list": []any{"a", "<b>", "c\"d"}, "obj": map[string]any{"k": "v<"}}
		if f[1] != "" {
			for _, a := range split(f[1], ';') {
				kv := split(a, '=')
				name := unescape(kv[0])
				v := ""
				if len(kv) > 1 {
					v = kv[1]
				}
				t, val := byte('s'), ""
				if len(v) > 0 {
					t = v[0]
				}
				if len(v) > 2 {
					val = unescape(v[2:])
				}
				switch t {
				case 'i':
					n, _ := strconv.ParseInt(val, 10, 64)
					data[name] = n
				case 'd':
					d, _ := strconv.ParseFloat(val, 64)
					data[name] = d
				case 'b':
					data[name] = val == "1"
				case 'n':
					data[name] = nil
				default:
					data[name] = val
				}
			}
		}
		t, err := template.New("x").Funcs(funcs).Parse(unescape(f[0]))
		var b strings.Builder
		if err == nil {
			err = t.Execute(&b, data)
		}
		if err != nil {
			fmt.Fprintln(w, "ERROR")
			continue
		}
		s := strings.NewReplacer("\\", "\\\\", "\t", "\\t", "\n", "\\n").Replace(b.String())
		fmt.Fprintln(w, s)
	}
}
