package dev.rteam.visuals.modules.player;

import dev.rteam.visuals.Hooks;
import dev.rteam.visuals.api.Keys;
import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.KeySetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

/** Hold a key to zoom in smoothly. */
public final class Zoom extends Module {
	private final KeySetting zoomKey = add(new KeySetting("zoom_key", Lang.of("Zoom key (hold)", "Клавиша зума (зажать)"), Keys.KEY_C));
	private final NumberSetting factor = add(new NumberSetting("factor", Lang.of("Zoom", "Приближение"), 4.0, 1.5, 15.0, 0.5).suffix("x"));
	private final BoolSetting smooth = add(new BoolSetting("smooth", Lang.of("Cinematic camera", "Плавная камера"), true));

	private static Zoom instance;
	private float current = 1f;
	private boolean smoothSet;
	private boolean savedSmooth;

	public Zoom() {
		super("zoom", Lang.of("Zoom", "Зум"), Lang.of("Hold C to look far (like OptiFine)", "Зажмите C, чтобы приблизить (как в OptiFine)"), Category.PLAYER);
		instance = this;
		setEnabled(true, false);
	}

	private boolean zooming() {
		return isEnabled() && Mc.screen() == null && Keys.isDown(zoomKey.get());
	}

	/** Multiplier for the field of view, called while rendering. */
	public static double fovMultiplier() {
		if (instance == null) return 1.0;
		return instance.update();
	}

	private double update() {
		boolean z = zooming();
		float target = z ? 1f / factor.f() : 1f;
		current = MathUtil.approach(current, target, 14f, Hooks.frameDelta());
		if (smooth.on()) {
			if (z && !smoothSet) {
				savedSmooth = Mc.mc().options.smoothCamera;
				Mc.mc().options.smoothCamera = true;
				smoothSet = true;
			} else if (!z && smoothSet) {
				Mc.mc().options.smoothCamera = savedSmooth;
				smoothSet = false;
			}
		}
		return current;
	}
}
