package dev.rteam.visuals.modules.player;

import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;

/** Third person camera distance and passing through blocks. */
public final class CameraTweaks extends Module {
	private final NumberSetting distance = add(new NumberSetting("distance", Lang.of("Distance", "Дистанция"), 4.0, 1.0, 12.0, 0.5));
	private final BoolSetting noClip = add(new BoolSetting("noclip", Lang.of("Through blocks", "Сквозь блоки"), true));

	private static CameraTweaks instance;

	public CameraTweaks() {
		super("camera", Lang.of("Camera", "Камера"), Lang.of("Third person distance, camera goes through blocks", "Дистанция камеры от третьего лица, проход сквозь блоки"), Category.PLAYER);
		instance = this;
	}

	public static double distance(double original) {
		if (instance == null || !instance.isEnabled()) return original;
		return instance.distance.d();
	}

	public static boolean noClip() {
		return instance != null && instance.isEnabled() && instance.noClip.on();
	}
}
