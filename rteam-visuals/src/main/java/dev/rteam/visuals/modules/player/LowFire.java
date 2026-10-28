package dev.rteam.visuals.modules.player;

import com.mojang.blaze3d.vertex.PoseStack;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;

/** Moves the burning overlay down so it covers less of the screen. */
public final class LowFire extends Module {
	private final NumberSetting offset = add(new NumberSetting("offset", Lang.of("Lower by", "Опустить на"), 0.3, 0.0, 0.6, 0.02));

	private static LowFire instance;
	private static float applied;

	public LowFire() {
		super("lowfire", Lang.of("Low Fire", "Низкий огонь"), Lang.of("Fire on screen covers less", "Огонь на экране меньше мешает"), Category.PLAYER);
		instance = this;
	}

	public static void start(PoseStack pose) {
		applied = instance != null && instance.isEnabled() ? instance.offset.f() : 0f;
		if (applied != 0f) pose.translate(0, -applied, 0);
	}

	public static void end(PoseStack pose) {
		if (applied != 0f) pose.translate(0, applied, 0);
		applied = 0f;
	}
}
