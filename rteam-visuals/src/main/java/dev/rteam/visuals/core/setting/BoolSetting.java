package dev.rteam.visuals.core.setting;

import com.google.gson.JsonElement;
import com.google.gson.JsonPrimitive;
import dev.rteam.visuals.util.Lang;

public final class BoolSetting extends Setting<Boolean> {
	public BoolSetting(String id, Lang name, boolean def) {
		super(id, name, def);
	}

	public boolean on() {
		return value;
	}

	public void toggle() {
		set(!value);
	}

	@Override
	public JsonElement save() {
		return new JsonPrimitive(value);
	}

	@Override
	public void load(JsonElement e) {
		if (e != null && e.isJsonPrimitive()) set(e.getAsBoolean());
	}
}
