package dev.rteam.visuals.core.setting;

import com.google.gson.JsonElement;
import com.google.gson.JsonPrimitive;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

public final class NumberSetting extends Setting<Double> {
	private final double min;
	private final double max;
	private final double step;
	private String suffix = "";

	public NumberSetting(String id, Lang name, double def, double min, double max, double step) {
		super(id, name, def);
		this.min = min;
		this.max = max;
		this.step = step;
	}

	public NumberSetting suffix(String s) {
		this.suffix = s;
		return this;
	}

	public double min() {
		return min;
	}

	public double max() {
		return max;
	}

	public double step() {
		return step;
	}

	public String suffix() {
		return suffix;
	}

	public float f() {
		return value.floatValue();
	}

	public int i() {
		return (int) Math.round(value);
	}

	public double d() {
		return value;
	}

	@Override
	public void set(Double v) {
		if (v == null || v.isNaN()) return;
		super.set(MathUtil.clamp(MathUtil.roundTo(v, step), min, max));
	}

	/** 0..1 position of the current value inside the range. */
	public double fraction() {
		return max == min ? 0 : (value - min) / (max - min);
	}

	public void setFraction(double f) {
		set(min + (max - min) * MathUtil.clamp(f, 0.0, 1.0));
	}

	public String display() {
		return MathUtil.format(value, MathUtil.decimalsOf(step)) + suffix;
	}

	@Override
	public JsonElement save() {
		return new JsonPrimitive(value);
	}

	@Override
	public void load(JsonElement e) {
		if (e != null && e.isJsonPrimitive()) set(e.getAsDouble());
	}
}
