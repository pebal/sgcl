// Same shape as benchmarks/encoding/json.cpp: JSON and CSV of Go's
// encoding packages over the same texts.
//   json [op=parse] [corpus=twitter] [seconds=2]
// ops: parse (json/v2 Unmarshal into any), parse1 (v1), write (v2 Marshal
// of the any), pretty (with an indent of two), tokens (jsontext
// ReadToken), skip (jsontext.Value.IsValid); typed and stringify (v2 of a
// slice of structs, corpus = the count of records), csv, csvtyped (with
// strconv per field) and csvwrite (the same records as CSV)
// A corpus is ~/Programming/oracles/nativejson/<corpus>.json, or
// "strings", made here as the C++ side makes it.
// Prints nanoseconds per call and megabytes of text per second.
package main

import (
	"bytes"
	"encoding/csv"
	"encoding/json"
	"encoding/json/jsontext"
	jsonv2 "encoding/json/v2"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

var sink int

type record struct {
	ID     int64    `json:"id"`
	Name   string   `json:"name"`
	Email  string   `json:"email"`
	Score  float64  `json:"score"`
	Active bool     `json:"active"`
	Tags   []string `json:"tags"`
}

func records(n int) []record {
	out := make([]record, n)
	for i := range out {
		out[i] = record{ID: int64(i * 7919), Name: "user " + strconv.Itoa(i), Email: "user" + strconv.Itoa(i) + "@example.com",
			Score: float64(i%1000) / 8.0, Active: i%3 == 0, Tags: []string{"t" + strconv.Itoa(i%5), "group, " + strconv.Itoa(i%11)}}
	}
	return out
}

func csvText(rs []record) []byte {
	var sb strings.Builder
	sb.WriteString("id,name,email,score,active\n")
	for _, r := range rs {
		b, _ := jsonv2.Marshal(r.Score)
		sb.WriteString(strconv.FormatInt(r.ID, 10) + "," + r.Name + "," + r.Email + "," + string(b) + "," + strconv.FormatBool(r.Active) + "\n")
	}
	return []byte(sb.String())
}

func corpus(name string) []byte {
	if name == "strings" {
		var sb strings.Builder
		sb.WriteByte('[')
		for i := 0; i < 2000; i++ {
			if i > 0 {
				sb.WriteByte(',')
			}
			sb.WriteByte('"')
			sb.WriteString(strings.Repeat(string(rune('a'+i%26)), 200+i%800))
			sb.WriteString("\\n")
			sb.WriteString(strings.Repeat("q", 100))
			sb.WriteByte('"')
		}
		sb.WriteByte(']')
		return []byte(sb.String())
	}
	home, _ := os.UserHomeDir()
	data, err := os.ReadFile(filepath.Join(home, "Programming", "oracles", "nativejson", name+".json"))
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	return data
}

// Calls of f for about `seconds`, after one call not timed
func timed(seconds float64, f func()) (float64, int) {
	f()
	t0 := time.Now()
	count := 0
	for {
		f()
		count++
		if time.Since(t0).Seconds() >= seconds {
			break
		}
	}
	return float64(time.Since(t0).Nanoseconds()) / float64(count), count
}

func main() {
	op, name, seconds := "parse", "twitter", 2.0
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	if len(os.Args) > 2 {
		name = os.Args[2]
	}
	if len(os.Args) > 3 {
		seconds, _ = strconv.ParseFloat(os.Args[3], 64)
	}
	var data []byte
	n, _ := strconv.Atoi(name)
	if n == 0 {
		n = 10000
	}
	switch op {
	case "typed", "stringify":
		data, _ = jsonv2.Marshal(records(n))
	case "csv", "csvtyped", "csvwrite":
		data = csvText(records(n))
	default:
		data = corpus(name)
	}
	var ns float64
	var count int
	switch op {
	case "typed":
		ns, count = timed(seconds, func() {
			var v []record
			if err := jsonv2.Unmarshal(data, &v); err != nil {
				panic(err)
			}
			sink += len(v)
		})
	case "stringify":
		rs := records(n)
		ns, count = timed(seconds, func() {
			b, _ := jsonv2.Marshal(rs)
			sink += len(b)
		})
	case "csv":
		ns, count = timed(seconds, func() {
			r := csv.NewReader(bytes.NewReader(data))
			for {
				rec, err := r.Read()
				if err == io.EOF {
					break
				}
				sink += len(rec)
			}
		})
	case "csvtyped":
		ns, count = timed(seconds, func() {
			r := csv.NewReader(bytes.NewReader(data))
			r.Read()
			for {
				rec, err := r.Read()
				if err == io.EOF {
					break
				}
				var x struct {
					id     int64
					name   string
					email  string
					score  float64
					active bool
				}
				x.id, _ = strconv.ParseInt(rec[0], 10, 64)
				x.name = rec[1]
				x.email = rec[2]
				x.score, _ = strconv.ParseFloat(rec[3], 64)
				x.active, _ = strconv.ParseBool(rec[4])
				sink += int(x.id)
			}
		})
	case "csvwrite":
		rs := records(n)
		rows := make([][]string, len(rs))
		for i, r := range rs {
			b, _ := jsonv2.Marshal(r.Score)
			rows[i] = []string{strconv.FormatInt(r.ID, 10), r.Name, r.Email, string(b), strconv.FormatBool(r.Active)}
		}
		ns, count = timed(seconds, func() {
			var buf bytes.Buffer
			w := csv.NewWriter(&buf)
			for _, r := range rows {
				w.Write(r)
			}
			w.Flush()
			sink += buf.Len()
		})
	case "parse":
		ns, count = timed(seconds, func() {
			var v any
			if err := jsonv2.Unmarshal(data, &v); err != nil {
				panic(err)
			}
			sink++
		})
	case "parse1":
		ns, count = timed(seconds, func() {
			var v any
			if err := json.Unmarshal(data, &v); err != nil {
				panic(err)
			}
			sink++
		})
	case "write", "pretty":
		var v any
		if err := jsonv2.Unmarshal(data, &v); err != nil {
			panic(err)
		}
		ns, count = timed(seconds, func() {
			var b []byte
			if op == "pretty" {
				b, _ = jsonv2.Marshal(v, jsontext.WithIndent("  "))
			} else {
				b, _ = jsonv2.Marshal(v)
			}
			sink += len(b)
		})
	case "tokens":
		ns, count = timed(seconds, func() {
			d := jsontext.NewDecoder(bytes.NewReader(data))
			for {
				t, err := d.ReadToken()
				if err == io.EOF {
					break
				}
				if err != nil {
					panic(err)
				}
				sink += int(t.Kind())
			}
		})
	case "skip":
		ns, count = timed(seconds, func() {
			if jsontext.Value(data).IsValid() {
				sink++
			}
		})
	default:
		fmt.Fprintln(os.Stderr, "json: no op called", op)
		os.Exit(2)
	}
	fmt.Printf("go op=%s corpus=%s count=%d ns/op=%.0f MB/s=%.0f\n", op, name, count, ns, float64(len(data))/ns*1e3)
}
