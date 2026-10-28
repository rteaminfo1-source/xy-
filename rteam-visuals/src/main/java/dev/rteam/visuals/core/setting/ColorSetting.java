package dev.rteam.visuals.core.setting;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;

/**
 * A color. Can follow the menu theme ("sync") or cycle through the rainbow.
 */
public final class ColorSetting extends Setting<Integer> {
	private boolean rainbow;
	private boolean sync;
	private final boolean allowSync;

	public ColorSetting(String id, Lang name, int argb, boolean syncWithTheme) {
		super(id, name, argb);
		this.sync = syncWithTheme;
		this.allowSync = true;
	}

	public ColorSetting(String id, Lang name, int argb) {
		this(id, name, argb, false);
	}

	/** Color to draw with right now (theme / rainbow applied). offset shifts gradients and rainbows. */
	public int color(float offset) {
		int a = ColorUtil.alpha(value);
		if (rainbow) return ColorUtil.withAlpha(ColorUtil.rainbow(1f, offset, 0.65f, 1f), a);
		if (sync) return ColorUtil.withAlpha(Theme.gradient(offset), a);
		return value;
	}

	public int color() {
		return color(0f);
	}

	public boolean rainbow() {
		return rainbow;
	}

	public void setRainbow(boolean r) {
		this.rainbow = r;
		if (r) this.sync = false;
	}

	public boolean sync() {
		return sync;
	}

	public void setSync(boolean s) {
		this.sync = s;
		if (s) this.rainbow = false;
	}

	public boolean allowSync() {
		return allowSync;
	}

	@Override
	public JsonElement save() {
		JsonObject o = new JsonObject();
		o.add("argb", new JsonPrimitive(ColorUtil.toHex(value)));
		o.add("rainbow", new JsonPrimitive(rainbow));
		o.add("sync", new JsonPrimitive(sync));
		return o;
	}

	@Override
	public void load(JsonElement e) {
		if (e == null) return;
		if (e.isJsonPrimitive()) {
			set(ColorUtil.parseHex(e.getAsString(), value));
			return;
		}
		if (!e.isJsonObject()) return;
		JsonObject o = e.getAsJsonObject();
		if (o.has("argb")) set(ColorUtil.parseHex(o.get("argb").getAsString(), value));
		rainbow = o.has("rainbow") && o.get("rainbow").getAsBoolean();
		sync = o.has("sync") && o.get("sync").getAsBoolean();
	}
}
