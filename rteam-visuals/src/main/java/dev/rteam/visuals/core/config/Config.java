package dev.rteam.visuals.core.config;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import dev.rteam.visuals.RteamVisuals;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.core.setting.Setting;
import net.fabricmc.loader.api.FabricLoader;

import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/** Saves everything to config/rteamvisuals.json. */
public final class Config {
	private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
	private static boolean dirty;
	private static long dirtySince;

	private Config() {
	}

	public static Path file() {
		return FabricLoader.getInstance().getConfigDir().resolve("rteamvisuals.json");
	}

	public static void markDirty() {
		if (!dirty) dirtySince = System.currentTimeMillis();
		dirty = true;
	}

	/** Saves at most once per second after a change; call every tick. */
	public static void tick() {
		if (dirty && System.currentTimeMillis() - dirtySince > 1000L) save();
	}

	public static void load() {
		Path path = file();
		if (!Files.exists(path)) return;
		try (Reader r = Files.newBufferedReader(path, StandardCharsets.UTF_8)) {
			JsonObject root = GSON.fromJson(r, JsonObject.class);
			if (root == null) return;
			if (root.has("client") && root.get("client").isJsonObject()) {
				ClientSettings.load(root.getAsJsonObject("client"));
			}
			JsonObject modules = root.has("modules") ? root.getAsJsonObject("modules") : new JsonObject();
			for (Module m : ModuleManager.all()) {
				if (!modules.has(m.id())) continue;
				JsonObject o = modules.getAsJsonObject(m.id());
				JsonObject settings = o.has("settings") ? o.getAsJsonObject("settings") : new JsonObject();
				for (Setting<?> s : m.settings()) {
					JsonElement e = settings.get(s.id());
					if (e != null) {
						try {
							s.load(e);
						} catch (RuntimeException ex) {
							RteamVisuals.LOG.warn("[RTeam Visuals] bad value for " + m.id() + "." + s.id());
						}
					}
				}
				if (o.has("key")) m.key.load(o.get("key"));
				if (m instanceof HudModule && o.has("pos")) ((HudModule) m).loadPosition(o.getAsJsonObject("pos"));
				if (o.has("enabled")) m.setEnabled(o.get("enabled").getAsBoolean(), false);
			}
		} catch (IOException | RuntimeException e) {
			RteamVisuals.LOG.error("[RTeam Visuals] could not read " + path, e);
		}
		dirty = false;
	}

	public static void save() {
		dirty = false;
		JsonObject root = new JsonObject();
		root.addProperty("version", 1);
		root.add("client", ClientSettings.save());
		JsonObject modules = new JsonObject();
		for (Module m : ModuleManager.all()) {
			JsonObject o = new JsonObject();
			o.addProperty("enabled", m.isEnabled());
			o.add("key", m.key.save());
			JsonObject settings = new JsonObject();
			for (Setting<?> s : m.settings()) settings.add(s.id(), s.save());
			o.add("settings", settings);
			if (m instanceof HudModule) {
				JsonObject pos = new JsonObject();
				((HudModule) m).savePosition(pos);
				o.add("pos", pos);
			}
			modules.add(m.id(), o);
		}
		root.add("modules", modules);
		Path path = file();
		try {
			Files.createDirectories(path.getParent());
			Path tmp = path.resolveSibling("rteamvisuals.json.tmp");
			try (Writer w = Files.newBufferedWriter(tmp, StandardCharsets.UTF_8)) {
				GSON.toJson(root, w);
			}
			Files.move(tmp, path, StandardCopyOption.REPLACE_EXISTING);
		} catch (IOException | RuntimeException e) {
			RteamVisuals.LOG.error("[RTeam Visuals] could not save " + path, e);
		}
	}
}
