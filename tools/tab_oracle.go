// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of sgcl/txt/tab_writer.h: Go's text/tabwriter asked the same
// questions. Only a tool, never in the library; tools/tab_vectors.py runs it
// (go run tools/tab_oracle.go) over the cases it writes.
//
// A question is a line of space-separated fields: min_width, tab_width,
// padding, the pad byte and the flags as numbers, then the text in hex and
// the sizes of the pieces it is written in (comma-separated, "-" for one
// piece). The answer is the output in hex.
package main

import (
	"bufio"
	"bytes"
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
	"strings"
	"text/tabwriter"
)

func main() {
	in := bufio.NewScanner(os.Stdin)
	in.Buffer(make([]byte, 1<<20), 1<<26)
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	for in.Scan() {
		f := strings.Fields(in.Text())
		n := make([]int, 5)
		for i := 0; i < 5; i++ {
			n[i], _ = strconv.Atoi(f[i])
		}
		text, _ := hex.DecodeString(f[5])
		var buf bytes.Buffer
		w := tabwriter.NewWriter(&buf, n[0], n[1], n[2], byte(n[3]), uint(n[4]))
		if f[6] == "-" {
			w.Write(text)
		} else {
			at := 0
			for _, s := range strings.Split(f[6], ",") {
				k, _ := strconv.Atoi(s)
				w.Write(text[at : at+k])
				at += k
			}
			w.Write(text[at:])
		}
		w.Flush()
		fmt.Fprintln(out, hex.EncodeToString(buf.Bytes()))
	}
}
