package dev.rteam.visuals.modules.render;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.render.R3D;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.Lang;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/** Expanding rings on the ground where you jump. */
public final class JumpCircles extends Module {
	private final NumberSetting size = add(new NumberSetting("size", Lang.of("Size", "Размер"), 1.4, 0.5, 3.0, 0.1));
	private final NumberSetting time = add(new NumberSetting("time", Lang.of("Duration", "Длительность"), 1.2, 0.3, 3.0, 0.1).suffix(Lang.t("s", " с")));
	private final ColorMode colors = new ColorMode(this, add(ColorMode.modeSetting()),
			add(new ColorSetting("color", Lang.of("Color", "Цвет"), 0xFF6EC8FF)));

	private static final class Circle {
		final double x, y, z;
		final long start = System.currentTimeMillis();

		Circle(double x, double y, double z) {
			this.x = x;
			this.y = y;
			this.z = z;
		}
	}

	private final List<Circle> circles = new ArrayList<>();

	public JumpCircles() {
		super("jumpcircles", Lang.of("Jump Circles", "Круги прыжка"), Lang.of("Rings on the ground when you jump", "Расходящиеся круги на земле при прыжке"), Category.RENDER);
	}

	@Override
	public void onJump() {
		if (Mc.player() == null) return;
		circles.add(new Circle(Mc.player().getX(), Mc.player().getY() + 0.02, Mc.player().getZ()));
		if (circles.size() > 12) circles.remove(0);
	}

	@Override
	public void onWorldJoin() {
		circles.clear();
	}

	@Override
	public void onRender3D(float pt) {
		long life = (long) (time.d() * 1000);
		Iterator<Circle> it = circles.iterator();
		while (it.hasNext()) {
			Circle c = it.next();
			float t = (System.currentTimeMillis() - c.start) / (float) life;
			if (t >= 1f) {
				it.remove();
				continue;
			}
			float grow = Anim.Easing.OUT_CUBIC.apply(t);
			float alpha = 1f - Anim.Easing.IN_OUT_CUBIC.apply(t);
			double r = 0.2 + size.d() * grow;
			double rot = t * 120.0;
			// soft glow band inside the ring and a bright outer edge
			R3D.ringGradient(c.x, c.y, c.z, r, Math.min(r, 0.45), colors::at, 0f, 0.45f * alpha, 64, rot);
			R3D.ringGradient(c.x, c.y + 0.001, c.z, r, 0.06, colors::at, 0.9f * alpha, 0.9f * alpha, 64, rot);
		}
	}
}
