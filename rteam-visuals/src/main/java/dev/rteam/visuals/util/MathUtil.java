package dev.rteam.visuals.util;

public final class MathUtil {
	private MathUtil() {
	}

	public static float clamp(float v, float min, float max) {
		return v < min ? min : (v > max ? max : v);
	}

	public static double clamp(double v, double min, double max) {
		return v < min ? min : (v > max ? max : v);
	}

	public static int clamp(int v, int min, int max) {
		return v < min ? min : (v > max ? max : v);
	}

	public static float lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}

	public static double lerp(double a, double b, double t) {
		return a + (b - a) * t;
	}

	/** Frame-rate independent exponential approach: moves current towards target. */
	public static float approach(float current, float target, float speed, float deltaSeconds) {
		float k = 1f - (float) Math.exp(-speed * deltaSeconds);
		float v = current + (target - current) * k;
		return Math.abs(target - v) < 0.0005f ? target : v;
	}

	public static float wrapDegrees(float deg) {
		deg %= 360f;
		if (deg >= 180f) deg -= 360f;
		if (deg < -180f) deg += 360f;
		return deg;
	}

	public static double roundTo(double v, double step) {
		if (step <= 0) return v;
		return Math.round(v / step) * step;
	}

	public static String format(double v, int decimals) {
		if (decimals <= 0) return Long.toString(Math.round(v));
		return String.format(java.util.Locale.ROOT, "%." + decimals + "f", v);
	}

	public static int decimalsOf(double step) {
		if (step >= 1) return 0;
		String s = java.math.BigDecimal.valueOf(step).stripTrailingZeros().toPlainString();
		int dot = s.indexOf('.');
		return dot < 0 ? 0 : s.length() - dot - 1;
	}
}
