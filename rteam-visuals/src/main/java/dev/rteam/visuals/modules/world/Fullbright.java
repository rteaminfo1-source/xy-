package dev.rteam.visuals.modules.world;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;

/** Everything bright: the lightmap is built with a high gamma (the video setting is not touched). */
public final class Fullbright extends Module {
	private final NumberSetting level = add(new NumberSetting("level", Lang.of("Brightness", "Яркость"), 10.0, 1.0, 16.0, 0.5));

	private static Fullbright instance;
	private static boolean swapped;
	private static Object savedValue;
	private static double savedDouble;

	public Fullbright() {
		super("fullbright", Lang.of("Fullbright", "Полная яркость"), Lang.of("See in the dark", "Видно даже в темноте"), Category.WORLD);
		instance = this;
	}

	public static boolean active() {
		return instance != null && instance.isEnabled();
	}

	public static float gamma() {
		return instance == null ? 1f : instance.level.f();
	}

	/** Raises gamma just for the lightmap update. */
	public static void push() {
		if (!active() || swapped) return;
		//#if MC >= 1.19
		dev.rteam.visuals.mixin.OptionInstanceAccessor acc = (dev.rteam.visuals.mixin.OptionInstanceAccessor) (Object) Mc.mc().options.gamma();
		savedValue = acc.rv$getValue();
		acc.rv$setValue(Double.valueOf(gamma()));
		//#else
		savedDouble = Mc.mc().options.gamma;
		Mc.mc().options.gamma = gamma();
		//#endif
		swapped = true;
	}

	public static void pop() {
		if (!swapped) return;
		//#if MC >= 1.19
		((dev.rteam.visuals.mixin.OptionInstanceAccessor) (Object) Mc.mc().options.gamma()).rv$setValue(savedValue);
		//#else
		Mc.mc().options.gamma = savedDouble;
		//#endif
		swapped = false;
	}
}
