package dev.rteam.visuals.modules.render;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.render.R3D;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/** A fading ribbon behind the player. */
public final class Trails extends Module {
	private final ModeSetting style = add(new ModeSetting("style", Lang.of("Style", "Стиль"), 0,
			new String[]{"ribbon", "line"}, new Lang[]{Lang.of("Ribbon", "Лента"), Lang.of("Line", "Линия")}));
	private final NumberSetting length = add(new NumberSetting("length", Lang.of("Length", "Длина"), 1.0, 0.2, 3.0, 0.1).suffix(Lang.t("s", " с")));
	private final NumberSetting height = add(new NumberSetting("height", Lang.of("Height", "Высота"), 1.0, 0.1, 2.0, 0.05))
			.visibleWhen(() -> style.is("ribbon"));
	private final NumberSetting opacity = add(new NumberSetting("opacity", Lang.of("Opacity", "Непрозрачность"), 0.6, 0.1, 1.0, 0.05));
	private final ColorMode colors = new ColorMode(this, add(ColorMode.modeSetting()),
			add(new ColorSetting("color", Lang.of("Color", "Цвет"), 0xFFFF6AD5)));

	private static final class Point {
		final double x, y, z;
		final long time = System.currentTimeMillis();

		Point(double x, double y, double z) {
			this.x = x;
			this.y = y;
			this.z = z;
		}
	}

	private final ArrayDeque<Point> points = new ArrayDeque<>();

	public Trails() {
		super("trails", Lang.of("Trails", "Шлейф"), Lang.of("A colored trail behind you", "Цветной шлейф за игроком"), Category.RENDER);
	}

	@Override
	protected void onDisable() {
		points.clear();
	}

	@Override
	public void onWorldJoin() {
		points.clear();
	}

	@Override
	public void onRender3D(float pt) {
		if (Mc.player() == null) return;
		double x = Mc.lerpX(Mc.player(), pt), y = Mc.lerpY(Mc.player(), pt), z = Mc.lerpZ(Mc.player(), pt);
		Point last = points.peekLast();
		if (last == null || (x - last.x) * (x - last.x) + (y - last.y) * (y - last.y) + (z - last.z) * (z - last.z) > 0.0025) {
			points.addLast(new Point(x, y, z));
		}
		long life = (long) (length.d() * 1000);
		long now = System.currentTimeMillis();
		Iterator<Point> it = points.iterator();
		while (it.hasNext()) {
			if (now - it.next().time > life) it.remove();
			else break;
		}
		if (points.size() < 2) return;
		List<Point> list = new ArrayList<>(points);
		boolean firstPerson = Mc.mc().options.getCameraType().isFirstPerson();
		float op = opacity.f();
		double h = height.d();
		for (int i = 0; i < list.size() - 1; i++) {
			Point a = list.get(i), b = list.get(i + 1);
			float fa = 1f - (now - a.time) / (float) life;
			float fb = 1f - (now - b.time) / (float) life;
			// in first person the newest part would cover the view
			if (firstPerson && i >= list.size() - 4) continue;
			int ca = ColorUtil.fade(colors.at(i / (double) list.size()), fa * op);
			int cb = ColorUtil.fade(colors.at((i + 1) / (double) list.size()), fb * op);
			if (style.is("ribbon")) {
				R3D.quad(a.x, a.y, a.z, ca, a.x, a.y + h, a.z, ColorUtil.fade(ca, 0.1f),
						b.x, b.y + h, b.z, ColorUtil.fade(cb, 0.1f), b.x, b.y, b.z, cb);
			} else {
				R3D.line(a.x, a.y + 0.9, a.z, b.x, b.y + 0.9, b.z, 0.08f, ca, cb);
			}
		}
	}
}
