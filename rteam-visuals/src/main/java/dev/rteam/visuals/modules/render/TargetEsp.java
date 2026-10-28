package dev.rteam.visuals.modules.render;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.render.R3D;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.TargetTracker;
import net.minecraft.world.entity.LivingEntity;

/** Animated marker around the entity you are fighting (only where it is visible, not through walls). */
public final class TargetEsp extends Module {
	private final ModeSetting style = add(new ModeSetting("style", Lang.of("Style", "Стиль"), 0,
			new String[]{"ring", "ghosts", "circle"},
			new Lang[]{Lang.of("Ring", "Кольцо"), Lang.of("Ghosts", "Призраки"), Lang.of("Ground circle", "Круг на земле")}));
	private final NumberSetting speed = add(new NumberSetting("speed", Lang.of("Speed", "Скорость"), 1.0, 0.2, 3.0, 0.1));
	private final ColorMode colors = new ColorMode(this, add(ColorMode.modeSetting()),
			add(new ColorSetting("color", Lang.of("Color", "Цвет"), 0xFFFF4D6D)));

	private final Anim appear = new Anim(0f, 300, Anim.Easing.OUT_CUBIC);
	private LivingEntity last;

	public TargetEsp() {
		super("targetesp", Lang.of("Target ESP", "Метка цели"), Lang.of("Animated marker on your target", "Анимированная метка на противнике"), Category.RENDER);
	}

	@Override
	public String suffix() {
		return style.display();
	}

	@Override
	public void onRender3D(float pt) {
		LivingEntity t = TargetTracker.target();
		if (t != null) last = t;
		appear.to(t != null ? 1f : 0f);
		float a = appear.get();
		if (last == null || a <= 0.01f || Mc.level() == null) return;
		LivingEntity e = last;
		double x = Mc.lerpX(e, pt), y = Mc.lerpY(e, pt), z = Mc.lerpZ(e, pt);
		double w = e.getBbWidth() * 0.75 + 0.15, h = e.getBbHeight();
		double time = (System.currentTimeMillis() % 1000000L) / 1000.0 * speed.d();
		float hurt = Math.min(1f, e.hurtTime / 10f);
		if (style.is("ring")) {
			double k = (Math.sin(time * 2.2) * 0.5 + 0.5);
			double ry = y + h * k;
			R3D.ringGradient(x, ry, z, w, 0.045, f -> ColorUtil.lerp(colors.at(f), 0xFFFF3030, hurt), a, a, 64, time * 90);
			// fading wall that trails behind the moving ring
			double dir = Math.cos(time * 2.2) >= 0 ? -1 : 1;
			R3D.wall(x, ry, z, w, 0.35 * dir, colors::at, 0.5f * a, 0f, 64, time * 90);
		} else if (style.is("ghosts")) {
			for (int i = 0; i < 3; i++) {
				double ph = time * 2.4 + i * Math.PI * 2 / 3;
				for (int j = 0; j < 8; j++) {
					double pj = ph - j * 0.12;
					double gx = x + Math.cos(pj) * w;
					double gz = z + Math.sin(pj) * w;
					double gy = y + h * 0.5 + Math.sin(pj * 1.3 + i) * h * 0.35;
					float f = (1f - j / 8f) * a;
					double s = 0.16 * (1f - j / 10f);
					int c = ColorUtil.fade(colors.at(i / 3.0), f);
					R3D.line(gx, gy - s, gz, gx, gy + s, gz, (float) (s * 2), c, c);
				}
			}
		} else {
			R3D.ringGradient(x, y + 0.02, z, w + 0.1, 0.08, colors::at, a, a, 64, time * 60);
			R3D.ringGradient(x, y + 0.02, z, w + 0.1, w + 0.1, colors::at, 0f, 0.25f * a, 64, time * 60);
		}
	}
}
