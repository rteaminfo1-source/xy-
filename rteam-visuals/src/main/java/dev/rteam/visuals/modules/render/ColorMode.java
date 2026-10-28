package dev.rteam.visuals.modules.render;

import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;

/** "Theme / Rainbow / Custom" color choice shared by the world effects. */
public final class ColorMode {
	public final ModeSetting mode;
	public final ColorSetting custom;

	public ColorMode(Module owner, ModeSetting mode, ColorSetting custom) {
		this.mode = mode;
		this.custom = custom;
	}

	public static ModeSetting modeSetting() {
		return new ModeSetting("color_mode", Lang.of("Colors", "Цвета"), 0,
				new String[]{"theme", "rainbow", "custom"},
				new Lang[]{Lang.of("Theme", "Тема"), Lang.of("Rainbow", "Радуга"), Lang.of("Custom", "Свой")});
	}

	/** Color at a position 0..1 along the effect (used for gradients around circles). */
	public int at(double f) {
		if (mode.is("rainbow")) return ColorUtil.rainbow(1f, (float) f, 0.65f, 1f);
		if (mode.is("custom")) return custom.color((float) f);
		// two-color theme gradient, symmetric so the ends meet without a seam
		float t = (float) (Math.sin(f * Math.PI * 2.0 + System.currentTimeMillis() / 600.0) * 0.5 + 0.5);
		return ColorUtil.lerp(Theme.accent1, Theme.accent2, t);
	}
}
