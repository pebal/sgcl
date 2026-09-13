// Same shape as benchmarks/write_barrier.cpp: the cost of copying a
// reference. mode stack: a local takes objs[i] (ZGC's load barrier on the
// read, no barrier on a local); mode heap: a field of a heap object takes
// objs[i], through the store barrier of the generational ZGC. targets:
// distinct referents cycled through; shared: every thread copies references
// to the same objects.
//   java WriteBarrier [threads=1] [mode=stack|heap] [targets=1] [shared]
public final class WriteBarrier {
    static final class Node { long v; Node next; }

    static final long ITERS = 50_000_000L;
    static final Object[] sink = new Object[256 * 16];   // one slot per thread, 128 bytes apart

    public static void main(String[] args) throws Exception {
        int threads = args.length > 0 ? Integer.parseInt(args[0]) : 1;
        String mode = args.length > 1 ? args[1] : "stack";
        int targets = args.length > 2 ? Integer.parseInt(args[2]) : 1;
        boolean shared = args.length > 3 && args[3].equals("shared");
        Node[] sharedObjs = new Node[targets];
        if (shared) for (int i = 0; i < targets; ++i) sharedObjs[i] = new Node();
        Thread[] ws = new Thread[threads];
        long t0 = System.nanoTime();
        for (int t = 0; t < threads; ++t) {
            final int id = t;
            ws[t] = new Thread(() -> {
                Node[] objs = sharedObjs;
                if (!shared) {
                    objs = new Node[targets];
                    for (int i = 0; i < targets; ++i) objs[i] = new Node();
                }
                Node holder = new Node();
                if (mode.equals("stack")) {
                    Node dst = null;
                    for (long i = 0; i < ITERS; ++i) dst = objs[(int) (i % targets)];
                    sink[id * 16] = dst;
                } else {
                    for (long i = 0; i < ITERS; ++i) holder.next = objs[(int) (i % targets)];
                    sink[id * 16] = holder;
                }
            });
            ws[t].start();
        }
        for (Thread w : ws) w.join();
        double ns = (System.nanoTime() - t0) / (double) ITERS;
        System.out.printf("java threads=%d mode=%s targets=%d%s ns/copy=%.2f cpu=%.2fs%n", threads, mode, targets, shared ? " shared" : "", ns, Common.cpuSeconds());
    }
}
