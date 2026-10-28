package dev.rteam.visuals.core.config;

import com.google.gson.JsonObject;

/** Global options that are not modules (language, light/dark, last opened category...). */
public final class ClientSettings {
	private ClientSettings() {
	}

	public static String lastCategory = "HUD";
	public static String lastModule = "";
	public static float menuX = Float.NaN;
	public static float menuY = Float.NaN;

	static JsonObject save() {
		JsonObject o = new JsonObject();
		o.addProperty("lastCategory", lastCategory);
		o.addProperty("lastModule", lastModule);
		if (!Float.isNaN(menuX)) {
			o.addProperty("menuX", menuX);
			o.addProperty("menuY", menuY);
		}
		return o;
	}

	static void load(JsonObject o) {
		if (o.has("lastCategory")) lastCategory = o.get("lastCategory").getAsString();
		if (o.has("lastModule")) lastModule = o.get("lastModule").getAsString();
		if (o.has("menuX") && o.has("menuY")) {
			menuX = o.get("menuX").getAsFloat();
			menuY = o.get("menuY").getAsFloat();
		}
	}
}
