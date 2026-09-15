// Same shape as benchmarks/string.cpp: the cost of a String kept in
// objects (immutable, a reference; the hash computed once and kept).
//   java Strings [op=make|copy|hash1|hashn] [len=10]
//   make: 2 M strings made from tokens of a byte buffer, each stored in a node
//   copy: 2 M references copied from one node to another, in order
//   hash1: 2 M strings each hashed once (hashCode: computed and kept)
//   hashn: 2 M strings each hashed eight times in a row (the seven after the first from the cache)
// Prints nanoseconds per operation. Every round makes the strings again,
// so that the first hash is a first hash; the second of two rounds is
// printed, the JIT warm.
import java.nio.charset.StandardCharsets;
import java.util.Random;

public final class Strings {
    static final class Node { String s; }

    static final int COUNT = 2_000_000;
    static long sink;

    public static void main(String[] args) {
        String op = args.length > 0 ? args[0] : "make";
        int len = args.length > 1 ? Integer.parseInt(args[1]) : 10;
        Random rng = new Random(1);
        byte[] buffer = new byte[COUNT * len];
        for (int i = 0; i < buffer.length; ++i) buffer[i] = (byte) ('a' + rng.nextInt(26));
        Node[] nodes = new Node[COUNT];
        for (int i = 0; i < COUNT; ++i) nodes[i] = new Node();
        Node[] targets = new Node[COUNT];
        for (int i = 0; i < COUNT; ++i) targets[i] = new Node();
        double ns = 0;
        for (int round = 0; round < 2; ++round) {
            System.gc();
            long t0 = System.nanoTime();
            for (int i = 0; i < COUNT; ++i) nodes[i].s = new String(buffer, i * len, len, StandardCharsets.ISO_8859_1);
            double make = (System.nanoTime() - t0) / (double) COUNT;
            switch (op) {
                case "make":
                    ns = make;
                    break;
                case "copy": {
                    // once untimed: ZGC's load barrier remaps every reference it meets after a collection, and that is not a copy's cost
                    for (int i = 0; i < COUNT; ++i) targets[i].s = nodes[i].s;
                    t0 = System.nanoTime();
                    for (int i = 0; i < COUNT; ++i) targets[i].s = nodes[i].s;
                    ns = (System.nanoTime() - t0) / (double) COUNT;
                    sink += targets[7].s.length();
                    break;
                }
                default: {
                    int times = op.equals("hashn") ? 8 : 1;
                    long sum = 0;
                    t0 = System.nanoTime();
                    for (int i = 0; i < COUNT; ++i) {
                        for (int k = 0; k < times; ++k) sum += nodes[i].s.hashCode();
                    }
                    ns = (System.nanoTime() - t0) / (double) COUNT / times;
                    sink += sum;
                }
            }
        }
        System.out.printf("java op=%s len=%d ns/op=%.2f cpu=%.2fs%n", op, len, ns, Common.cpuSeconds());
    }
}
