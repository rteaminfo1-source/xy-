package dev.rteam.visuals.core.setting;

import com.google.gson.JsonElement;
import com.google.gson.JsonPrimitive;
import dev.rteam.visuals.util.Lang;

public final class TextSetting extends Setting<String> {
	private final int maxLength;

	public TextSetting(String id, Lang name, String def, int maxLength) {
		super(id, name, def);
		this.maxLength = maxLength;
	}

	public int maxLength() {
		return maxLength;
	}

	@Override
	public void set(String v) {
		if (v == null) return;
		super.set(v.length() > maxLength ? v.substring(0, maxLength) : v);
	}

	@Override
	public JsonElement save() {
		return new JsonPrimitive(value);
	}

	@Override
	public void load(JsonElement e) {
		if (e != null && e.isJsonPrimitive()) set(e.getAsString());
	}
}
