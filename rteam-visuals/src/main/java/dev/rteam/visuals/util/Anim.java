package dev.rteam.visuals.util;

/**
 * Time based animation of one value from 0..1 (or any range) with easing.
 * Call {@link #to(float)} to change the target; {@link #get()} returns the eased current value.
 */
public final class Anim {
	private float from;
	private float target;
	private long start;
	private final long durationMs;
	private final Easing easing;

	public Anim(float initial, long durationMs, Easing easing) {
		this.from = initial;
		this.target = initial;
		this.durationMs = Math.max(1L, durationMs);
		this.easing = easing;
		this.start = 0L;
	}

	public Anim(long durationMs) {
		this(0f, durationMs, Easing.OUT_CUBIC);
	}

	public void to(float value) {
		if (value == target) return;
		this.from = get();
		this.target = value;
		this.start = System.currentTimeMillis();
	}

	public void set(float value) {
		this.from = value;
		this.target = value;
		this.start = 0L;
	}

	public float target() {
		return target;
	}

	public boolean done() {
		return progress() >= 1f;
	}

	private float progress() {
		if (start == 0L) return 1f;
		float p = (System.currentTimeMillis() - start) / (float) (durationMs * Anim.speedFactor);
		return p >= 1f ? 1f : (p < 0f ? 0f : p);
	}

	public float get() {
		float p = progress();
		if (p >= 1f) return target;
		return from + (target - from) * easing.apply(p);
	}

	/** Global animation speed multiplier (menu setting). 1 = normal, 2 = twice as slow. */
	public static float speedFactor = 1f;

	public enum Easing {
		LINEAR {
			@Override
			public float apply(float t) {
				return t;
			}
		},
		OUT_QUAD {
			@Override
			public float apply(float t) {
				return 1f - (1f - t) * (1f - t);
			}
		},
		OUT_CUBIC {
			@Override
			public float apply(float t) {
				float u = 1f - t;
				return 1f - u * u * u;
			}
		},
		OUT_QUART {
			@Override
			public float apply(float t) {
				float u = 1f - t;
				return 1f - u * u * u * u;
			}
		},
		IN_OUT_CUBIC {
			@Override
			public float apply(float t) {
				return t < 0.5f ? 4f * t * t * t : 1f - (float) Math.pow(-2f * t + 2f, 3) / 2f;
			}
		},
		OUT_BACK {
			@Override
			public float apply(float t) {
				float c1 = 1.70158f, c3 = c1 + 1f;
				float u = t - 1f;
				return 1f + c3 * u * u * u + c1 * u * u;
			}
		},
		OUT_ELASTIC {
			@Override
			public float apply(float t) {
				if (t == 0f || t == 1f) return t;
				double c4 = (2 * Math.PI) / 3;
				return (float) (Math.pow(2, -10 * t) * Math.sin((t * 10 - 0.75) * c4) + 1);
			}
		};

		public abstract float apply(float t);
	}
}
