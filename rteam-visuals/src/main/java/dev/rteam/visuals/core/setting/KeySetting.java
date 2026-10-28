package dev.rteam.visuals.core.setting;

import com.google.gson.JsonElement;
import com.google.gson.JsonPrimitive;
import dev.rteam.visuals.util.Lang;

/** Keyboard key code (GLFW/SDL key code as reported by Minecraft); -1 = none. */
public final class KeySetting extends Setting<Integer> {
	public static final int NONE = -1;

	public KeySetting(String id, Lang name, int key) {
		super(id, name, key);
	}

	public boolean bound() {
		return value != NONE;
	}

	@Override
	public JsonElement save() {
		return new JsonPrimitive(value);
	}

	@Override
	public void load(JsonElement e) {
		if (e != null && e.isJsonPrimitive()) set(e.getAsInt());
	}
}
