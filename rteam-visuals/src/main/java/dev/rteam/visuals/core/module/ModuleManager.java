package dev.rteam.visuals.core.module;

import dev.rteam.visuals.RteamVisuals;
import dev.rteam.visuals.core.config.Config;
import dev.rteam.visuals.gui.Notifications;
import dev.rteam.visuals.util.Lang;
import net.minecraft.world.entity.Entity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class ModuleManager {
	private static final List<Module> MODULES = new ArrayList<>();
	private static final Map<Class<?>, Module> BY_CLASS = new HashMap<>();
	private static final Set<Module> FAILED = new HashSet<>();

	private ModuleManager() {
	}

	public static <M extends Module> M register(M module) {
		MODULES.add(module);
		BY_CLASS.put(module.getClass(), module);
		return module;
	}

	public static List<Module> all() {
		return Collections.unmodifiableList(MODULES);
	}

	public static List<Module> byCategory(Category c) {
		List<Module> out = new ArrayList<>();
		for (Module m : MODULES) if (m.category() == c) out.add(m);
		return out;
	}

	@SuppressWarnings("unchecked")
	public static <M extends Module> M get(Class<M> type) {
		return (M) BY_CLASS.get(type);
	}

	public static boolean on(Class<? extends Module> type) {
		Module m = BY_CLASS.get(type);
		return m != null && m.isEnabled();
	}

	public static Module byId(String id) {
		for (Module m : MODULES) if (m.id().equals(id)) return m;
		return null;
	}

	/** A module threw: log once, switch it off so the game keeps running. */
	public static void report(Module m, Throwable t) {
		if (FAILED.add(m)) {
			RteamVisuals.LOG.error("[RTeam Visuals] module " + m.id() + " failed and was disabled", t);
		}
		if (m.isEnabled()) {
			m.setEnabled(false, false);
			Notifications.push(m.name(), Lang.t("Error, module disabled", "Ошибка, модуль выключен"), 0xFFFF5555);
		}
	}

	static void onToggled(Module m) {
		Notifications.toggled(m);
		Config.markDirty();
	}

	// ---- dispatch -------------------------------------------------------------------------------

	public static void tick() {
		for (int i = 0; i < MODULES.size(); i++) {
			Module m = MODULES.get(i);
			if (!m.isEnabled()) continue;
			try {
				m.onTick();
			} catch (Throwable t) {
				report(m, t);
			}
		}
	}

	public static void render2D(float partialTick) {
		for (int i = 0; i < MODULES.size(); i++) {
			Module m = MODULES.get(i);
			if (!m.isEnabled()) continue;
			try {
				m.onRender2D(partialTick);
			} catch (Throwable t) {
				report(m, t);
			}
		}
	}

	public static void render3D(float partialTick) {
		for (int i = 0; i < MODULES.size(); i++) {
			Module m = MODULES.get(i);
			if (!m.isEnabled()) continue;
			try {
				m.onRender3D(partialTick);
			} catch (Throwable t) {
				report(m, t);
			}
		}
	}

	public static void attack(Entity target) {
		for (int i = 0; i < MODULES.size(); i++) {
			Module m = MODULES.get(i);
			if (!m.isEnabled()) continue;
			try {
				m.onAttack(target);
			} catch (Throwable t) {
				report(m, t);
			}
		}
	}

	public static void jump() {
		for (int i = 0; i < MODULES.size(); i++) {
			Module m = MODULES.get(i);
			if (!m.isEnabled()) continue;
			try {
				m.onJump();
			} catch (Throwable t) {
				report(m, t);
			}
		}
	}

	public static void worldJoin() {
		for (int i = 0; i < MODULES.size(); i++) {
			Module m = MODULES.get(i);
			try {
				m.onWorldJoin();
			} catch (Throwable t) {
				report(m, t);
			}
		}
	}

	/** Called when a key is pressed with no screen open. */
	public static void keyPressed(int key) {
		for (int i = 0; i < MODULES.size(); i++) {
			Module m = MODULES.get(i);
			if (m.key.bound() && m.key.get() == key) m.toggle();
		}
	}
}
