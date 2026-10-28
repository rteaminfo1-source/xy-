package dev.rteam.visuals.modules.player;

import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.util.Lang;

/** No camera shake when you take damage. */
public final class NoHurtCam extends Module {
	private static NoHurtCam instance;

	public NoHurtCam() {
		super("nohurtcam", Lang.of("No Hurt Cam", "Без тряски камеры"), Lang.of("The camera does not shake when you are hit", "Камера не трясётся при получении урона"), Category.PLAYER);
		instance = this;
	}

	public static boolean active() {
		return instance != null && instance.isEnabled();
	}
}
