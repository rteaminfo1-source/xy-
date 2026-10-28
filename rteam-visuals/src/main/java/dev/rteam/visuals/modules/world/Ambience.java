package dev.rteam.visuals.modules.world;

import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;

/** Client side time of day. */
public final class Ambience extends Module {
	private final ModeSetting mode = add(new ModeSetting("mode", Lang.of("Mode", "Режим"), 0,
			new String[]{"fixed", "cycle"}, new Lang[]{Lang.of("Fixed time", "Фиксированное время"), Lang.of("Fast cycle", "Быстрый цикл")}));
	private final NumberSetting time = add(new NumberSetting("time", Lang.of("Time (hours)", "Время (часы)"), 18.0, 0.0, 23.5, 0.5).suffix(":00"))
			.visibleWhen(() -> mode.is("fixed"));
	private final NumberSetting speed = add(new NumberSetting("speed", Lang.of("Cycle speed", "Скорость цикла"), 10.0, 1.0, 100.0, 1.0))
			.visibleWhen(() -> mode.is("cycle"));

	private static Ambience instance;

	public Ambience() {
		super("ambience", Lang.of("Ambience", "Время суток"), Lang.of("Day or night on your screen only", "День или ночь только у вас на экране"), Category.WORLD);
		instance = this;
	}

	@Override
	public String suffix() {
		return mode.is("fixed") ? (int) time.d() + ":00" : "x" + speed.i();
	}

	public static long dayTime(long original) {
		if (instance == null || !instance.isEnabled()) return original;
		long day = original - Math.floorMod(original, 24000L);
		if (instance.mode.is("cycle")) {
			return day + (long) ((System.currentTimeMillis() / 50.0 * instance.speed.d()) % 24000.0);
		}
		// Minecraft day starts at 6:00
		long ticks = (long) ((instance.time.d() - 6.0) * 1000.0);
		return day + Math.floorMod(ticks, 24000L);
	}
}
