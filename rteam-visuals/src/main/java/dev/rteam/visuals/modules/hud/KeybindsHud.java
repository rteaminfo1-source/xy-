package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Keys;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.modules.misc.ClickGuiModule;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;

import java.util.ArrayList;
import java.util.List;

/** Modules that have a key bound, with their state. */
public final class KeybindsHud extends HudModule {
	public KeybindsHud() {
		super("keybinds", Lang.of("Keybinds", "Бинды"), Lang.of("Modules with keys and their state", "Модули с клавишами и их состояние"), 0.01f, 0.2f);
	}

	@Override
	protected void draw(float pt, boolean editor) {
		List<Module> list = new ArrayList<>();
		for (Module m : ModuleManager.all()) {
			if (m.key.bound() && !(m instanceof ClickGuiModule)) list.add(m);
		}
		float w = 100;
		for (Module m : list) w = Math.max(w, R2D.width(m.name()) + R2D.width(Keys.name(m.key.get())) + 30);
		float h = 16 + Math.max(1, list.size()) * 12 + 2;
		HudStyle.accentPanel(0, 0, w, h);
		HudStyle.header(Lang.t("Keybinds", "Бинды"), 0, 0, w);
		float y = 16;
		if (list.isEmpty()) {
			R2D.text(Lang.t("No keys bound", "Нет назначенных клавиш"), 6, y + 1, Theme.textFaint());
		}
		for (Module m : list) {
			R2D.circle(8, y + 5, 2.2f, m.isEnabled() ? 0xFF4ADE80 : ColorUtil.withAlpha(0x888888, 200));
			R2D.text(m.name(), 14, y + 1, m.isEnabled() ? Theme.text() : Theme.textDim());
			String k = Keys.name(m.key.get());
			float kw = R2D.width(k) + 6;
			R2D.roundRect(w - 5 - kw, y - 0.5f, kw, 10, 3, Theme.stroke());
			R2D.text(k, w - 2 - kw, y + 1, Theme.textDim());
			y += 12;
		}
		width = w;
		height = h;
	}
}
