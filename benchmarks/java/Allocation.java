// Allocation throughput, the shape of benchmarks/allocation.cpp: each
// thread allocates n objects of `size` bytes of payload keeping only the
// newest (stored into a slot of its own, so the allocation is real).
//   java Allocation [threads=1] [size=32] [n=20000000]
public final class Allocation {
    static final class Obj8 { long a; }
    static final class Obj32 { long a, b, c, d; }
    static final class Obj256 { long a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, a30, a31; }

    static final Object[] sinks = new Object[256 * 16];   // one slot per thread, 128 bytes apart

    public static void main(String[] args) throws Exception {
        int threads = args.length > 0 ? Integer.parseInt(args[0]) : 1;
        int size = args.length > 1 ? Integer.parseInt(args[1]) : 32;
        long n = args.length > 2 ? Long.parseLong(args[2]) : 20_000_000L;
        Thread[] ws = new Thread[threads];
        long[] nanos = new long[threads];
        for (int t = 0; t < threads; ++t) {
            final int id = t;
            ws[t] = new Thread(() -> {
                long t0 = System.nanoTime();
                int slot = id * 16;
                switch (size) {
                    case 8: for (long i = 0; i < n; ++i) { var o = new Obj8(); o.a = i; sinks[slot] = o; } break;
                    case 256: for (long i = 0; i < n; ++i) { var o = new Obj256(); o.a0 = i; sinks[slot] = o; } break;
                    default: for (long i = 0; i < n; ++i) { var o = new Obj32(); o.a = i; sinks[slot] = o; } break;
                }
                nanos[id] = System.nanoTime() - t0;
            });
            ws[t].start();
        }
        long max = 0;
        for (int t = 0; t < threads; ++t) { ws[t].join(); max = Math.max(max, nanos[t]); }
        System.out.printf("java threads=%d size=%d ns/alloc=%.2f cpu=%.2fs%n", threads, size, (double) max / n, Common.cpuSeconds());
    }
}
