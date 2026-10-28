package dev.rteam.visuals.modules.player;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;
import net.minecraft.world.entity.LivingEntity;

/** Slower or faster hand swing (only how it looks). */
public final class SwingAnimation extends Module {
	private final NumberSetting speed = add(new NumberSetting("speed", Lang.of("Swing length", "Длительность взмаха"), 1.6, 0.4, 4.0, 0.1).suffix("x"));

	private static SwingAnimation instance;

	public SwingAnimation() {
		super("swing", Lang.of("Swing Animation", "Анимация удара"), Lang.of("Slow or fast hand swing", "Медленный или быстрый взмах рукой"), Category.PLAYER);
		instance = this;
	}

	public static int duration(LivingEntity entity, int original) {
		if (instance == null || !instance.isEnabled() || entity != Mc.player()) return original;
		return Math.max(1, Math.round(original * instance.speed.f()));
	}
}
