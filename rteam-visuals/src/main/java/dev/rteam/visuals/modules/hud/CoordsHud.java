package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

/** Coordinates, nether/overworld conversion and facing direction. */
public final class CoordsHud extends HudModule {
	private final BoolSetting nether = add(new BoolSetting("nether", Lang.of("Other dimension", "Координаты другого мира"), true));
	private final BoolSetting direction = add(new BoolSetting("direction", Lang.of("Direction", "Направление"), true));

	public CoordsHud() {
		super("coords", Lang.of("Coordinates", "Координаты"), Lang.of("Position, nether coordinates and direction", "Позиция, координаты ада и направление"), 0.01f, 0.98f);
		setEnabled(true, false);
	}

	private static String dir(float yaw) {
		float y = MathUtil.wrapDegrees(yaw);
		if (y >= -45 && y < 45) return Lang.t("South +Z", "Юг +Z");
		if (y >= 45 && y < 135) return Lang.t("West -X", "Запад -X");
		if (y >= -135 && y < -45) return Lang.t("East +X", "Восток +X");
		return Lang.t("North -Z", "Север -Z");
	}

	@Override
	protected void draw(float pt, boolean editor) {
		double x = 0, y = 0, z = 0;
		float yaw = 0;
		if (Mc.player() != null) {
			x = Mc.player().getX();
			y = Mc.player().getY();
			z = Mc.player().getZ();
			yaw = Mc.yaw(Mc.player());
		}
		String line1 = "XYZ  " + (long) Math.floor(x) + "  " + (long) Math.floor(y) + "  " + (long) Math.floor(z);
		String line2 = null;
		if (nether.on()) {
			boolean inNether = Mc.inNether();
			double f = inNether ? 8 : 1 / 8.0;
			line2 = (inNether ? Lang.t("World", "Мир") : Lang.t("Nether", "Ад")) + "  " + (long) Math.floor(x * f) + "  " + (long) Math.floor(z * f);
		}
		String line3 = direction.on() ? dir(yaw) : null;
		float w = R2D.width(line1);
		if (line2 != null) w = Math.max(w, R2D.width(line2));
		if (line3 != null) w = Math.max(w, R2D.width(line3));
		w += 12;
		int lines = 1 + (line2 != null ? 1 : 0) + (line3 != null ? 1 : 0);
		float h = 6 + lines * 10;
		HudStyle.panel(0, 0, w, h);
		R2D.rect(3, 4, 4.5f, h - 4, Theme.gradient(0f));
		float ty = 4;
		R2D.text(line1, 8, ty, Theme.text());
		ty += 10;
		if (line2 != null) {
			R2D.text(line2, 8, ty, Theme.textDim());
			ty += 10;
		}
		if (line3 != null) R2D.text(line3, 8, ty, Theme.textDim());
		width = w;
		height = h;
	}
}
