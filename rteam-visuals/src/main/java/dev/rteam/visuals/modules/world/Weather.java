package dev.rteam.visuals.modules.world;

import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.util.Lang;

/** Client side weather. */
public final class Weather extends Module {
	private final ModeSetting mode = add(new ModeSetting("mode", Lang.of("Weather", "Погода"), 0,
			new String[]{"clear", "rain", "thunder"}, new Lang[]{Lang.of("Clear", "Ясно"), Lang.of("Rain", "Дождь"), Lang.of("Thunder", "Гроза")}));

	private static Weather instance;

	public Weather() {
		super("weather", Lang.of("Weather", "Погода"), Lang.of("Clear sky or rain on your screen only", "Ясно или дождь только у вас на экране"), Category.WORLD);
		instance = this;
	}

	@Override
	public String suffix() {
		return mode.display();
	}

	public static float rain(float original) {
		if (instance == null || !instance.isEnabled()) return original;
		return instance.mode.is("clear") ? 0f : 1f;
	}

	public static float thunder(float original) {
		if (instance == null || !instance.isEnabled()) return original;
		return instance.mode.is("thunder") ? 1f : 0f;
	}
}
