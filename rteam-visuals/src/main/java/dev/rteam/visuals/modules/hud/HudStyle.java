package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;

/** The common look of HUD elements. */
public final class HudStyle {
	private HudStyle() {
	}

	public static final float RADIUS = 5f;

	public static void panel(float x, float y, float w, float h) {
		R2D.shadow(x, y, w, h, RADIUS, 7, ColorUtil.withAlpha(0, 90));
		R2D.roundRect(x, y, w, h, RADIUS, Theme.hudBg());
	}

	/** Panel with a thin animated accent line on top. */
	public static void accentPanel(float x, float y, float w, float h) {
		panel(x, y, w, h);
		R2D.roundRectGradient(x + 4, y, w - 8, 1.5f, 0.75f, Theme.gradient(0f), Theme.gradient(0.5f), true);
	}

	/** Header line of a panel: title on the left, optional icon dot. */
	public static void header(String title, float x, float y, float w) {
		R2D.circle(x + 7, y + 7.5f, 2.2f, Theme.gradient(0f));
		R2D.text(title, x + 13, y + 4, Theme.text());
	}

	public static String roman(int n) {
		String[] r = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
		return n >= 0 && n < r.length ? r[n] : Integer.toString(n);
	}

	public static String time(int ticks) {
		int s = Math.max(0, ticks / 20);
		return s >= 3600 ? String.format("%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60) : String.format("%d:%02d", s / 60, s % 60);
	}
}
