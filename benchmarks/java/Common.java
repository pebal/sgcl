// Shared helpers of the Java counterparts of benchmarks/*.cpp: process CPU
// time and percentiles, the same shapes as common.h.
import java.lang.management.ManagementFactory;
import java.util.Arrays;

final class Common {
    static double cpuSeconds() {
        var os = (com.sun.management.OperatingSystemMXBean) ManagementFactory.getOperatingSystemMXBean();
        return os.getProcessCpuTime() / 1e9;
    }

    static double[] percentiles(int[] ns, int count) {
        int[] a = Arrays.copyOf(ns, count);
        Arrays.sort(a);
        if (count == 0) return new double[]{0, 0, 0, 0, 0, 0};
        return new double[]{a[(int) (count * 0.5)], a[(int) (count * 0.9)], a[(int) (count * 0.99)], a[(int) Math.min(count - 1, count * 0.999)], a[(int) Math.min(count - 1, count * 0.9999)], a[count - 1]};
    }
}
