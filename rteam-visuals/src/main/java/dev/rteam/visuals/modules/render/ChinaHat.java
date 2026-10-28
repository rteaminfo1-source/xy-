package dev.rteam.visuals.modules.render;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.render.R3D;
import dev.rteam.visuals.util.Lang;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;

/** A conical hat above the head. */
public final class ChinaHat extends Module {
	private final NumberSetting radius = add(new NumberSetting("radius", Lang.of("Radius", "Радиус"), 0.7, 0.3, 1.2, 0.05));
	private final NumberSetting height = add(new NumberSetting("height", Lang.of("Height", "Высота"), 0.3, 0.1, 0.8, 0.05));
	private final NumberSetting spin = add(new NumberSetting("spin", Lang.of("Rotation speed", "Скорость вращения"), 1.0, 0.0, 5.0, 0.1));
	private final NumberSetting opacity = add(new NumberSetting("opacity", Lang.of("Opacity", "Непрозрачность"), 0.7, 0.1, 1.0, 0.05));
	private final BoolSetting firstPerson = add(new BoolSetting("first_person", Lang.of("In first person", "От первого лица"), false));
	private final BoolSetting others = add(new BoolSetting("others", Lang.of("On other players", "На других игроках"), false));
	private final ColorMode colors = new ColorMode(this, add(ColorMode.modeSetting()),
			add(new ColorSetting("color", Lang.of("Color", "Цвет"), 0xFFB57BFF)));

	public ChinaHat() {
		super("chinahat", Lang.of("China Hat", "Китайская шляпа"), Lang.of("A glowing cone above your head", "Светящаяся шляпа-конус над головой"), Category.RENDER);
	}

	@Override
	public void onRender3D(float pt) {
		if (Mc.level() == null || Mc.player() == null) return;
		draw(Mc.player(), pt, true);
		if (others.on()) {
			for (Player p : Mc.level().players()) {
				if (p != Mc.player() && !p.isInvisible()) draw(p, pt, false);
			}
		}
	}

	private void draw(Entity e, float pt, boolean self) {
		if (self && !firstPerson.on() && Mc.mc().options.getCameraType().isFirstPerson()) return;
		double x = Mc.lerpX(e, pt), y = Mc.lerpY(e, pt) + e.getBbHeight() + 0.04, z = Mc.lerpZ(e, pt);
		double rot = (System.currentTimeMillis() % 100000L) / 1000.0 * 60.0 * spin.d();
		float a = opacity.f();
		R3D.cone(x, y, z, radius.d(), height.d(), colors::at, a, a * 0.55f, 48, rot);
		// thin bright rim
		R3D.ringGradient(x, y, z, radius.d() + 0.015, 0.03, colors::at, Math.min(1f, a + 0.2f), 0f, 48, rot);
	}
}
