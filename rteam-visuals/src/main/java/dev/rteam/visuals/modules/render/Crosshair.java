package dev.rteam.visuals.modules.render;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

/** Custom crosshair with attack cooldown indicator. */
public final class Crosshair extends Module {
	private final ModeSetting style = add(new ModeSetting("style", Lang.of("Style", "Стиль"), 0,
			new String[]{"cross", "dot", "circle", "crossdot"},
			new Lang[]{Lang.of("Cross", "Крест"), Lang.of("Dot", "Точка"), Lang.of("Circle", "Круг"), Lang.of("Cross + dot", "Крест + точка")}));
	private final NumberSetting gap = add(new NumberSetting("gap", Lang.of("Gap", "Зазор"), 2.0, 0.0, 8.0, 0.5));
	private final NumberSetting length = add(new NumberSetting("length", Lang.of("Length", "Длина"), 4.0, 1.0, 12.0, 0.5));
	private final NumberSetting thickness = add(new NumberSetting("thickness", Lang.of("Thickness", "Толщина"), 1.0, 0.5, 3.0, 0.25));
	private final BoolSetting dynamic = add(new BoolSetting("dynamic", Lang.of("Dynamic (moves apart)", "Динамический"), true));
	private final BoolSetting outline = add(new BoolSetting("outline", Lang.of("Outline", "Обводка"), true));
	private final BoolSetting cooldown = add(new BoolSetting("cooldown", Lang.of("Attack cooldown", "Перезарядка удара"), true));
	private final ColorSetting color = add(new ColorSetting("color", Lang.of("Color", "Цвет"), 0xFFFFFFFF));

	private static Crosshair instance;
	private float spread;

	public Crosshair() {
		super("crosshair", Lang.of("Crosshair", "Прицел"), Lang.of("Your own crosshair", "Собственный прицел"), Category.RENDER);
		instance = this;
	}

	public static boolean hideVanilla() {
		return instance != null && instance.isEnabled();
	}

	@Override
	public void onRender2D(float pt) {
		if (Mc.player() == null || !Mc.mc().options.getCameraType().isFirstPerson()) return;
		float cx = Mc.guiWidth() / 2f, cy = Mc.guiHeight() / 2f;
		double speed = Math.sqrt(Mc.velocity(Mc.player()).x * Mc.velocity(Mc.player()).x + Mc.velocity(Mc.player()).z * Mc.velocity(Mc.player()).z);
		float target = dynamic.on() ? (float) Math.min(4.0, speed * 12.0) : 0f;
		spread = MathUtil.approach(spread, target, 12f, dev.rteam.visuals.Hooks.frameDelta());
		float g = gap.f() + spread, l = length.f(), t = thickness.f();
		int c = color.color();
		int o = ColorUtil.withAlpha(0x000000, (int) (ColorUtil.alpha(c) * 0.6f));
		if (style.is("cross") || style.is("crossdot")) {
			if (outline.on()) {
				float e = 0.5f;
				R2D.rect(cx - t / 2 - e, cy - g - l - e, cx + t / 2 + e, cy - g + e, o);
				R2D.rect(cx - t / 2 - e, cy + g - e, cx + t / 2 + e, cy + g + l + e, o);
				R2D.rect(cx - g - l - e, cy - t / 2 - e, cx - g + e, cy + t / 2 + e, o);
				R2D.rect(cx + g - e, cy - t / 2 - e, cx + g + l + e, cy + t / 2 + e, o);
			}
			R2D.rect(cx - t / 2, cy - g - l, cx + t / 2, cy - g, c);
			R2D.rect(cx - t / 2, cy + g, cx + t / 2, cy + g + l, c);
			R2D.rect(cx - g - l, cy - t / 2, cx - g, cy + t / 2, c);
			R2D.rect(cx + g, cy - t / 2, cx + g + l, cy + t / 2, c);
		}
		if (style.is("dot") || style.is("crossdot")) {
			if (outline.on()) R2D.circle(cx, cy, t + 0.9f, o);
			R2D.circle(cx, cy, t + 0.4f, c);
		}
		if (style.is("circle")) {
			if (outline.on()) R2D.ring(cx, cy, g + l / 2 + 0.6f, t + 1.2f, o);
			R2D.ring(cx, cy, g + l / 2, t, c);
		}
		if (cooldown.on()) {
			float p = Mc.player().getAttackStrengthScale(0f);
			if (p < 1f) {
				float r = g + l + 3;
				R2D.arc(cx, cy, r, 1.5f, -90, -90 + 360 * p, ColorUtil.fade(c, 0.9f));
			}
		}
	}

	public static boolean active() {
		return ModuleManager.on(Crosshair.class);
	}
}
