// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// Same table as benchmarks/txt/tab_writer.cpp: 10,000 lines of four cells
// aligned by Go's text/tabwriter, whole and with the lines written one by
// one.
//   tab_writer [op=whole|lines] [rounds]
// Prints milliseconds per table.
package main

import (
	"bytes"
	"fmt"
	"os"
	"strconv"
	"strings"
	"text/tabwriter"
	"time"
)

func table() []string {
	lines := make([]string, 0, 10000)
	for i := 0; i < 10000; i++ {
		lines = append(lines, fmt.Sprintf("file_%d.txt\t%d\t%s\t2026-10-%02d\n", i, i*37%100000,
			strings.Repeat("rw-", 1+i%3), 1+i%28))
	}
	return lines
}

func main() {
	op := "whole"
	rounds := 50
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	if len(os.Args) > 2 {
		rounds, _ = strconv.Atoi(os.Args[2])
	}
	lines := table()
	whole := strings.Join(lines, "")
	sink := 0
	start := time.Now()
	for r := 0; r < rounds; r++ {
		var out bytes.Buffer
		w := tabwriter.NewWriter(&out, 0, 8, 1, ' ', 0)
		if op == "whole" {
			w.Write([]byte(whole))
		} else {
			for _, l := range lines {
				w.Write([]byte(l))
			}
		}
		w.Flush()
		sink += out.Len()
	}
	ms := float64(time.Since(start).Nanoseconds()) / 1e6 / float64(rounds)
	fmt.Printf("%s bytes=%d ms=%.3f (%d)\n", op, len(whole), ms, sink%7)
}
