package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/** List of enabled modules ("ArrayList") with slide animations. */
public final class ModuleList extends HudModule {
	private final ModeSetting colorMode = add(new ModeSetting("color", Lang.of("Colors", "Цвета"), 0,
			new String[]{"theme", "rainbow", "static"},
			new Lang[]{Lang.of("Theme", "Тема"), Lang.of("Rainbow", "Радуга"), Lang.of("Static", "Один цвет")}));
	private final ColorSetting staticColor = add(new ColorSetting("static_color", Lang.of("Color", "Цвет"), 0xFF8B5CF6))
			.visibleWhen(() -> colorMode.is("static"));
	private final BoolSetting background = add(new BoolSetting("background", Lang.of("Background", "Фон"), true));
	private final NumberSetting bgAlpha = add(new NumberSetting("bg_alpha", Lang.of("Background opacity", "Прозрачность фона"), 0.55, 0.0, 1.0, 0.05))
			.visibleWhen(background::on);
	private final BoolSetting sideBar = add(new BoolSetting("sidebar", Lang.of("Side line", "Боковая полоса"), true));
	private final BoolSetting lowercase = add(new BoolSetting("lowercase", Lang.of("Lowercase", "Строчные буквы"), false));
	private final BoolSetting suffixes = add(new BoolSetting("suffix", Lang.of("Show modes", "Показывать режимы"), true));
	private final ModeSetting sort = add(new ModeSetting("sort", Lang.of("Sort", "Сортировка"), 0,
			new String[]{"length", "alphabet"}, new Lang[]{Lang.of("By length", "По длине"), Lang.of("Alphabet", "По алфавиту")}));

	public ModuleList() {
		super("modulelist", Lang.of("Module List", "Список модулей"), Lang.of("Shows enabled modules", "Показывает включённые модули"), 0.99f, 0.01f);
		setEnabled(true, false);
	}

	private String label(Module m) {
		String s = m.listName();
		String suf = suffixes.on() ? m.suffix() : null;
		if (suf != null && !suf.isEmpty()) s = s + " §7" + suf;
		return lowercase.on() ? s.toLowerCase() : s;
	}

	private int color(int index, float offset) {
		if (colorMode.is("rainbow")) return ColorUtil.rainbow(1f, offset, 0.6f, 1f);
		if (colorMode.is("static")) return staticColor.color();
		return Theme.gradient(offset);
	}

	@Override
	protected void draw(float pt, boolean editor) {
		List<Module> list = new ArrayList<>();
		for (Module m : ModuleManager.all()) {
			if (!m.showInList()) continue;
			if (m.isEnabled() || m.listAnim.get() > 0.01f) list.add(m);
		}
		if (list.isEmpty() && editor) {
			list.add(this);
		}
		final boolean alpha = sort.is("alphabet");
		Collections.sort(list, new Comparator<Module>() {
			@Override
			public int compare(Module a, Module b) {
				if (alpha) return label(a).compareToIgnoreCase(label(b));
				return Integer.compare(R2D.width(label(b)), R2D.width(label(a)));
			}
		});
		boolean right = rightAligned();
		float lineH = 11f;
		float maxW = 0;
		for (Module m : list) maxW = Math.max(maxW, R2D.width(label(m)) + 8);
		float y = 0;
		int i = 0;
		for (Module m : list) {
			float a = (editor && list.size() == 1 && m == this) ? 1f : m.listAnim.get();
			if (a <= 0.01f) continue;
			String s = label(m);
			float tw = R2D.width(s);
			float w = tw + 8;
			float off = (1f - a) * (w + 6);
			float x = right ? maxW - w + off : -off;
			float h = lineH * Math.min(1f, a * 1.2f);
			float offset = i * 0.08f;
			int c = color(i, offset);
			R2D.setAlpha(Math.min(1f, a));
			if (background.on()) R2D.rect(x, y, x + w, y + h, ColorUtil.withAlpha(0x0C0C12, (int) (bgAlpha.d() * 255)));
			if (sideBar.on()) {
				float bx = right ? x + w : x - 1.5f;
				R2D.rect(bx, y, bx + 1.5f, y + h, c);
			}
			R2D.text(s, x + 4, y + 1.5f, c, true, 1f);
			R2D.setAlpha(1f);
			y += h;
			i++;
		}
		width = Math.max(30, maxW);
		height = Math.max(lineH, y);
	}
}
