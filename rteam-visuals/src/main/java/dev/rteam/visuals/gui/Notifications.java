package dev.rteam.visuals.gui;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.modules.hud.NotificationsModule;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/** Small toasts in the bottom right corner ("Module enabled", errors...). */
public final class Notifications {
	private Notifications() {
	}

	private static final class Toast {
		final String title;
		final String text;
		final int color;
		final long created = System.currentTimeMillis();
		final Anim slide = new Anim(0f, 320, Anim.Easing.OUT_BACK);
		final Anim y = new Anim(0f, 260, Anim.Easing.OUT_CUBIC);
		boolean closing;
		boolean placed;

		Toast(String title, String text, int color) {
			this.title = title;
			this.text = text;
			this.color = color;
			slide.to(1f);
		}
	}

	private static final List<Toast> TOASTS = new ArrayList<>();

	public static void push(String title, String text, int color) {
		synchronized (TOASTS) {
			TOASTS.add(new Toast(title, text, color));
			while (TOASTS.size() > 6) TOASTS.remove(0);
		}
	}

	public static void toggled(Module m) {
		NotificationsModule nm = ModuleManager.get(NotificationsModule.class);
		if (nm == null || !nm.isEnabled() || !nm.onToggle.on()) return;
		push(m.name(), m.isEnabled() ? Lang.t("Enabled", "Включён") : Lang.t("Disabled", "Выключен"),
				m.isEnabled() ? 0xFF4ADE80 : 0xFFF87171);
	}

	/** Drawn on top of the HUD (and in menus). */
	public static void render() {
		NotificationsModule nm = ModuleManager.get(NotificationsModule.class);
		long life = nm != null ? (long) (nm.duration.d() * 1000) : 2000L;
		float sw = Mc.guiWidth(), sh = Mc.guiHeight();
		float bottom = sh - 8;
		synchronized (TOASTS) {
			Iterator<Toast> it = TOASTS.iterator();
			while (it.hasNext()) {
				Toast t = it.next();
				if (!t.closing && System.currentTimeMillis() - t.created > life) {
					t.closing = true;
					t.slide.to(0f);
				}
				if (t.closing && t.slide.done()) it.remove();
			}
			for (int i = TOASTS.size() - 1; i >= 0; i--) {
				Toast t = TOASTS.get(i);
				float w = Math.max(R2D.width(t.title), R2D.width(t.text)) + 34;
				float h = 26;
				if (!t.placed) {
					t.y.set(bottom - h);
					t.placed = true;
				} else {
					t.y.to(bottom - h);
				}
				float y = t.y.get();
				float p = t.slide.get();
				float x = sw - (w + 8) * Math.max(0f, p);
				R2D.setAlpha(Math.min(1f, Math.max(0f, p)));
				R2D.shadow(x, y, w, h, 6, 8, Theme.shadow());
				R2D.roundRect(x, y, w, h, 6, Theme.panel());
				R2D.circle(x + 13, y + h / 2f, 7.5f, ColorUtil.fade(t.color, 0.25f));
				R2D.circle(x + 13, y + h / 2f, 4.5f, t.color);
				R2D.text(t.title, x + 24, y + 5, Theme.text());
				R2D.text(t.text, x + 24, y + 15, Theme.textDim());
				// time left
				float left = 1f - Math.min(1f, (System.currentTimeMillis() - t.created) / (float) life);
				R2D.roundRect(x + 6, y + h - 3, (w - 12) * left, 1.5f, 0.75f, ColorUtil.fade(t.color, 0.8f));
				R2D.setAlpha(1f);
				bottom = y - 4;
			}
		}
	}
}
