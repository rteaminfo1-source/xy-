package dev.rteam.visuals.util;

/** ARGB color helpers. All colors are packed 0xAARRGGBB ints. */
public final class ColorUtil {
	private ColorUtil() {
	}

	public static int alpha(int c) {
		return (c >>> 24) & 0xFF;
	}

	public static int red(int c) {
		return (c >> 16) & 0xFF;
	}

	public static int green(int c) {
		return (c >> 8) & 0xFF;
	}

	public static int blue(int c) {
		return c & 0xFF;
	}

	public static int argb(int a, int r, int g, int b) {
		return (clamp255(a) << 24) | (clamp255(r) << 16) | (clamp255(g) << 8) | clamp255(b);
	}

	public static int argb(float a, float r, float g, float b) {
		return argb(Math.round(a * 255f), Math.round(r * 255f), Math.round(g * 255f), Math.round(b * 255f));
	}

	private static int clamp255(int v) {
		return v < 0 ? 0 : (v > 255 ? 255 : v);
	}

	/** Same color with a new alpha (0..255). */
	public static int withAlpha(int c, int a) {
		return (clamp255(a) << 24) | (c & 0xFFFFFF);
	}

	/** Multiplies the alpha of a color by f (0..1). */
	public static int fade(int c, float f) {
		return withAlpha(c, Math.round(alpha(c) * MathUtil.clamp(f, 0f, 1f)));
	}

	public static int lerp(int a, int b, float t) {
		t = MathUtil.clamp(t, 0f, 1f);
		return argb(
				Math.round(alpha(a) + (alpha(b) - alpha(a)) * t),
				Math.round(red(a) + (red(b) - red(a)) * t),
				Math.round(green(a) + (green(b) - green(a)) * t),
				Math.round(blue(a) + (blue(b) - blue(a)) * t));
	}

	public static int darker(int c, float f) {
		return argb(alpha(c), Math.round(red(c) * f), Math.round(green(c) * f), Math.round(blue(c) * f));
	}

	public static int brighter(int c, float f) {
		return lerp(c, withAlpha(0xFFFFFF, alpha(c)), f);
	}

	/** h, s, v in 0..1 -> opaque RGB. */
	public static int hsv(float h, float s, float v) {
		h = h - (float) Math.floor(h);
		int i = (int) (h * 6f);
		float f = h * 6f - i;
		float p = v * (1f - s);
		float q = v * (1f - f * s);
		float t = v * (1f - (1f - f) * s);
		float r, g, b;
		switch (i % 6) {
			case 0: r = v; g = t; b = p; break;
			case 1: r = q; g = v; b = p; break;
			case 2: r = p; g = v; b = t; break;
			case 3: r = p; g = q; b = v; break;
			case 4: r = t; g = p; b = v; break;
			default: r = v; g = p; b = q; break;
		}
		return argb(1f, r, g, b);
	}

	/** RGB -> {h, s, v} in 0..1. */
	public static float[] toHsv(int c) {
		float r = red(c) / 255f, g = green(c) / 255f, b = blue(c) / 255f;
		float max = Math.max(r, Math.max(g, b));
		float min = Math.min(r, Math.min(g, b));
		float d = max - min;
		float h;
		if (d == 0f) h = 0f;
		else if (max == r) h = ((g - b) / d) % 6f;
		else if (max == g) h = (b - r) / d + 2f;
		else h = (r - g) / d + 4f;
		h /= 6f;
		if (h < 0f) h += 1f;
		float s = max == 0f ? 0f : d / max;
		return new float[]{h, s, max};
	}

	/** Moving rainbow; offset shifts the phase (e.g. per line of a list). */
	public static int rainbow(float speed, float offset, float saturation, float brightness) {
		double t = (System.currentTimeMillis() % 1_000_000L) / 1000.0;
		float hue = (float) ((t * speed * 0.15 + offset) % 1.0);
		return hsv(hue, saturation, brightness);
	}

	/** Smooth back-and-forth blend between two colors over time. */
	public static int wave(int a, int b, float speed, float offset) {
		double t = (System.currentTimeMillis() % 1_000_000L) / 1000.0;
		float k = (float) (Math.sin(t * speed * 2.0 + offset * Math.PI * 2.0) * 0.5 + 0.5);
		return lerp(a, b, k);
	}

	public static float luminance(int c) {
		return (0.2126f * red(c) + 0.7152f * green(c) + 0.0722f * blue(c)) / 255f;
	}

	public static String toHex(int c) {
		return String.format("#%08X", c);
	}

	public static int parseHex(String s, int fallback) {
		try {
			String h = s.trim();
			if (h.startsWith("#")) h = h.substring(1);
			if (h.length() == 6) h = "FF" + h;
			return (int) Long.parseLong(h, 16);
		} catch (RuntimeException e) {
			return fallback;
		}
	}
}
