package dev.rteam.visuals.gui.screen;

import dev.rteam.visuals.Hooks;
import dev.rteam.visuals.api.Keys;
import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.config.ClientSettings;
import dev.rteam.visuals.core.config.Config;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.KeySetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.core.setting.Setting;
import dev.rteam.visuals.core.setting.TextSetting;
import dev.rteam.visuals.gui.Icons;
import dev.rteam.visuals.gui.Notifications;
import dev.rteam.visuals.modules.misc.ClickGuiModule;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

/** The RTeam Visuals menu: categories, modules and their settings. */
public final class ClickGui implements View {
	private RvScreen screen;
	private int sw, sh;

	// window
	private float wx, wy, ww, wh;
	private boolean draggingWindow;
	private float dragDx, dragDy;
	private final Anim open = new Anim(0f, 280, Anim.Easing.OUT_BACK);
	private final Anim fade = new Anim(0f, 220, Anim.Easing.OUT_CUBIC);
	private boolean closing;

	// navigation
	private Category category = Category.HUD;
	private Module selected;
	private final Anim catIndicator = new Anim(0f, 260, Anim.Easing.OUT_CUBIC);
	private final Anim contentAnim = new Anim(1f, 260, Anim.Easing.OUT_CUBIC);
	private final Map<Object, Anim> hover = new HashMap<>();
	private final Map<Object, Anim> toggles = new HashMap<>();

	// search
	private String query = "";
	private boolean searchFocused;

	// scrolling
	private float listScroll, listScrollTarget, listContent;
	private float setScroll, setScrollTarget, setContent;

	// interaction state
	private NumberSetting draggingSlider;
	private float sliderX, sliderW;
	private KeySetting listeningKey;
	private TextSetting focusedText;
	private ColorSetting openColor;
	private float pickerX, pickerY;
	private int pickerDrag; // 0 none, 1 sv, 2 hue, 3 alpha
	private float[] pickerHsv = {0, 0, 1};

	// background particles
	private final float[][] particles = new float[46][5];
	private final Random random = new Random();

	// layout of the current frame (for hit tests)
	private float sideW, headerH, listX, listY, listW, listH, setX, setY, setW, setH;
	private final List<Hit> hits = new ArrayList<>();

	private static final int BTN_LEFT = 0;
	private static final int BTN_RIGHT = 1;

	/** Something clickable drawn this frame. */
	private interface Click {
		void run(double mx, double my, int button);
	}

	private static final class Hit {
		final float x1, y1, x2, y2;
		final Click click;

		Hit(float x1, float y1, float x2, float y2, Click click) {
			this.x1 = x1;
			this.y1 = y1;
			this.x2 = x2;
			this.y2 = y2;
			this.click = click;
		}

		boolean contains(double x, double y) {
			return x >= x1 && x <= x2 && y >= y1 && y <= y2;
		}
	}

	public static RvScreen create() {
		ClickGui gui = new ClickGui();
		RvScreen s = new RvScreen(gui);
		gui.screen = s;
		return s;
	}

	private ClickGui() {
		for (Category c : Category.values()) {
			if (c.name().equals(ClientSettings.lastCategory)) category = c;
		}
		Module last = ModuleManager.byId(ClientSettings.lastModule);
		selected = last != null ? last : firstOf(category);
		for (float[] p : particles) resetParticle(p, true);
	}

	private ClickGuiModule settingsModule() {
		return ModuleManager.get(ClickGuiModule.class);
	}

	private Module firstOf(Category c) {
		List<Module> l = ModuleManager.byCategory(c);
		return l.isEmpty() ? null : l.get(0);
	}

	private Anim anim(Map<Object, Anim> map, Object key, long ms) {
		Anim a = map.get(key);
		if (a == null) {
			a = new Anim(0f, ms, Anim.Easing.OUT_CUBIC);
			map.put(key, a);
		}
		return a;
	}

	@Override
	public void init(int width, int height) {
		sw = width;
		sh = height;
		open.to(1f);
		fade.to(1f);
		closing = false;
	}

	@Override
	public boolean vanillaBackground() {
		ClickGuiModule m = settingsModule();
		return m != null && m.blur.on();
	}

	@Override
	public void tick() {
	}

	@Override
	public boolean requestClose() {
		if (listeningKey != null) {
			listeningKey = null;
			return false;
		}
		if (openColor != null) {
			openColor = null;
			return false;
		}
		if (searchFocused || focusedText != null) {
			searchFocused = false;
			focusedText = null;
			screen.setTextInput(false);
			return false;
		}
		if (!closing) {
			closing = true;
			open.to(0f);
			fade.to(0f);
		}
		return false;
	}

	@Override
	public void removed() {
		ClientSettings.lastCategory = category.name();
		ClientSettings.lastModule = selected == null ? "" : selected.id();
		Config.save();
	}

	// ---- layout -----------------------------------------------------------------------------------

	private void layout() {
		sw = Mc.guiWidth();
		sh = Mc.guiHeight();
		ww = MathUtil.clamp(sw - 24f, 330f, 560f);
		wh = MathUtil.clamp(sh - 24f, 210f, 340f);
		if (Float.isNaN(ClientSettings.menuX)) {
			wx = (sw - ww) / 2f;
			wy = (sh - wh) / 2f;
		} else {
			wx = ClientSettings.menuX * sw - ww / 2f;
			wy = ClientSettings.menuY * sh - wh / 2f;
		}
		wx = MathUtil.clamp(wx, 4f, Math.max(4f, sw - ww - 4f));
		wy = MathUtil.clamp(wy, 4f, Math.max(4f, sh - wh - 4f));
		sideW = ww < 420 ? 98f : 120f;
		headerH = 36f;
		listX = wx + sideW + 8;
		listY = wy + headerH + 6;
		listW = (ww - sideW) * 0.43f - 10;
		listH = wh - headerH - 12;
		setX = listX + listW + 8;
		setY = wy + 8;
		setW = wx + ww - setX - 8;
		setH = wh - 16;
	}

	// ---- rendering --------------------------------------------------------------------------------

	@Override
	public void render(int mouseX, int mouseY, float delta) {
		if (closing && open.get() <= 0.02f && fade.get() <= 0.02f) {
			screen.closeNow();
			return;
		}
		layout();
		hits.clear();
		float dt = Hooks.frameDelta();
		listScroll = MathUtil.approach(listScroll, listScrollTarget, 16f, dt);
		setScroll = MathUtil.approach(setScroll, setScrollTarget, 16f, dt);
		if (draggingSlider != null) updateSlider(mouseX);

		float a = fade.get();
		// dim + particles
		R2D.setAlpha(a);
		R2D.gradientV(0, 0, sw, sh, ColorUtil.withAlpha(0x05050A, 110), ColorUtil.withAlpha(0x05050A, 170));
		ClickGuiModule m = settingsModule();
		if (m == null || m.particles.on()) drawParticles(dt);

		float s = 0.9f + 0.1f * open.get();
		R2D.push();
		R2D.scaleAround(s, wx + ww / 2f, wy + wh / 2f);
		drawWindow(mouseX, mouseY);
		R2D.pop();
		R2D.setAlpha(1f);
		if (openColor != null && !closing) {
			R2D.layer();
			drawColorPicker(mouseX, mouseY);
		}
		Notifications.render();
	}

	private void drawParticles(float dt) {
		for (float[] p : particles) {
			p[1] -= p[3] * dt * 12f;
			p[0] += (float) Math.sin((System.currentTimeMillis() / 1000.0 + p[4] * 10) * 0.6) * dt * 3f;
			if (p[1] < -5) resetParticle(p, false);
			int c = ColorUtil.fade(ColorUtil.lerp(Theme.accent1, Theme.accent2, p[4]), 0.12f + p[2] * 0.12f);
			R2D.circle(p[0], p[1], p[2], c);
		}
	}

	private void resetParticle(float[] p, boolean anywhere) {
		float w = Math.max(100, Mc.guiWidth()), h = Math.max(100, Mc.guiHeight());
		p[0] = random.nextFloat() * w;
		p[1] = anywhere ? random.nextFloat() * h : h + 5;
		p[2] = 0.8f + random.nextFloat() * 1.8f;
		p[3] = 0.5f + random.nextFloat() * 1.5f;
		p[4] = random.nextFloat();
	}

	private void drawWindow(int mx, int my) {
		float r = 10f;
		R2D.shadow(wx, wy, ww, wh, r, 16, ColorUtil.withAlpha(0, 140));
		R2D.roundRect(wx, wy, ww, wh, r, Theme.bg());
		// sidebar background
		R2D.roundRect4(wx, wy, sideW, wh, r, Theme.panel(), Theme.panel(), Theme.panel(), Theme.panel());
		R2D.rect(wx + sideW - r, wy, wx + sideW, wy + wh, Theme.bg());
		R2D.rect(wx + sideW - 1, wy + 10, wx + sideW, wy + wh - 10, Theme.stroke());
		// soft accent glow in the corner
		R2D.shadow(wx + 14, wy + 10, 22, 22, 11, 22, ColorUtil.withAlpha(Theme.accent1, 40));

		drawSidebar(mx, my);
		drawHeader(mx, my);
		drawModuleList(mx, my);
		drawSettings(mx, my);
	}

	// ---- sidebar ----------------------------------------------------------------------------------

	private void drawSidebar(int mx, int my) {
		// logo
		float lx = wx + 12, ly = wy + 12;
		R2D.roundRect4(lx, ly, 22, 22, 7, Theme.accent1, Theme.gradient(0.3f), Theme.accent2, Theme.gradient(0.8f));
		R2D.text("R", lx + 11 - R2D.width("R") * 1.25f / 2f, ly + 6, 0xFFFFFFFF, true, 1.25f);
		R2D.text("RTeam", lx + 28, ly + 2, Theme.text(), false, 1.1f);
		float phase = (System.currentTimeMillis() % 3000L) / 3000f;
		R2D.textGradient("Visuals", lx + 28, ly + 12.5f, Theme.accent1, Theme.accent2, false, 0.85f, phase);

		// categories
		float cy = wy + 50;
		float itemH = 24;
		int index = 0;
		Category[] cats = Category.values();
		for (int i = 0; i < cats.length; i++) {
			if (cats[i] == category) index = i;
		}
		catIndicator.to(index);
		float indY = cy + catIndicator.get() * (itemH + 2);
		if (query.isEmpty()) {
			R2D.roundRectGradient(wx + 8, indY, sideW - 16, itemH, 7,
					ColorUtil.withAlpha(Theme.accent1, 70), ColorUtil.withAlpha(Theme.accent2, 30), true);
			R2D.roundRect(wx + 8, indY + 6, 2.5f, itemH - 12, 1.25f, Theme.gradient(0f));
		}
		for (int i = 0; i < cats.length; i++) {
			final Category c = cats[i];
			float y = cy + i * (itemH + 2);
			boolean hov = in(mx, my, wx + 8, y, wx + sideW - 8, y + itemH);
			Anim h = anim(hover, c, 160);
			h.to(hov ? 1f : 0f);
			if (h.get() > 0.01f && c != category) {
				R2D.roundRect(wx + 8, y, sideW - 16, itemH, 7, ColorUtil.fade(Theme.panel3(), h.get()));
			}
			boolean sel = c == category && query.isEmpty();
			int col = sel ? Theme.text() : ColorUtil.lerp(Theme.textDim(), Theme.text(), h.get());
			Icons.draw(c.icon(), wx + 24, y + itemH / 2f, 10, sel ? Theme.gradient(0f) : col);
			R2D.text(c.displayName(), wx + 36, y + itemH / 2f - 4, col);
			hits.add(new Hit(wx + 8, y, wx + sideW - 8, y + itemH, (x, yy, b) -> selectCategory(c)));
		}

		// bottom buttons: HUD editor, theme, language
		float by = wy + wh - 30;
		float bx = wx + 8;
		float bw = sideW - 16;
		boolean hovEditor = in(mx, my, bx, by - 26, bx + bw, by - 6);
		R2D.roundRectGradient(bx, by - 26, bw, 20, 7, hovEditor ? Theme.accent1 : ColorUtil.withAlpha(Theme.accent1, 200),
				hovEditor ? Theme.accent2 : ColorUtil.withAlpha(Theme.accent2, 200), true);
		Icons.draw("layout", bx + 11, by - 16, 9, 0xFFFFFFFF);
		R2D.text(Lang.t("HUD Editor", "Редактор HUD"), bx + 20, by - 20, 0xFFFFFFFF);
		hits.add(new Hit(bx, by - 26, bx + bw, by - 6, (x, y, b) -> Mc.setScreen(HudEditor.create(this.screen))));

		float half = (bw - 4) / 2f;
		final ClickGuiModule m = settingsModule();
		boolean hovTheme = in(mx, my, bx, by, bx + half, by + 20);
		R2D.roundRect(bx, by, half, 20, 7, hovTheme ? Theme.panel3() : Theme.panel2());
		Icons.draw(Theme.dark ? "moon" : "sun", bx + half / 2f, by + 10, 10, Theme.text());
		hits.add(new Hit(bx, by, bx + half, by + 20, (x, y, b) -> {
			if (m != null) m.dark.toggle();
		}));
		boolean hovLang = in(mx, my, bx + half + 4, by, bx + bw, by + 20);
		R2D.roundRect(bx + half + 4, by, half, 20, 7, hovLang ? Theme.panel3() : Theme.panel2());
		String lang = Lang.russian ? "RU" : "EN";
		R2D.text(lang, bx + half + 4 + half / 2f - R2D.width(lang) / 2f, by + 6, Theme.text());
		hits.add(new Hit(bx + half + 4, by, bx + bw, by + 20, (x, y, b) -> {
			if (m != null) m.language.cycle(1);
		}));
	}

	private void selectCategory(Category c) {
		if (c == category && query.isEmpty()) return;
		category = c;
		query = "";
		listScrollTarget = 0;
		contentAnim.set(0f);
		contentAnim.to(1f);
		selected = firstOf(c);
		setScrollTarget = 0;
		openColor = null;
	}

	// ---- header -----------------------------------------------------------------------------------

	private void drawHeader(int mx, int my) {
		float hx = listX, hy = wy + 10;
		String title = query.isEmpty() ? category.displayName() : Lang.t("Search", "Поиск");
		R2D.text(title, hx + 2, hy + 3, Theme.text(), false, 1.3f);
		int count = visibleModules().size();
		String sub = count + " " + Lang.t("modules", count % 10 == 1 && count % 100 != 11 ? "модуль" : (count % 10 >= 2 && count % 10 <= 4 && (count % 100 < 10 || count % 100 >= 20) ? "модуля" : "модулей"));
		R2D.text(sub, hx + 4 + R2D.width(title) * 1.3f + 4, hy + 6, Theme.textFaint());

		// search box at the right of the module column
		float bw = Math.min(110, listW * 0.55f);
		float bx = listX + listW - bw, by = hy;
		boolean hov = in(mx, my, bx, by, bx + bw, by + 18);
		R2D.roundRect(bx, by, bw, 18, 9, searchFocused ? Theme.panel3() : (hov ? Theme.panel3() : Theme.panel2()));
		if (searchFocused) R2D.roundOutline(bx, by, bw, 18, 9, 1f, ColorUtil.withAlpha(Theme.accent1, 200));
		Icons.draw("search", bx + 10, by + 9, 9, Theme.textDim());
		String shown = query.isEmpty() && !searchFocused ? Lang.t("Search...", "Поиск...") : query;
		boolean caret = searchFocused && (System.currentTimeMillis() / 500) % 2 == 0;
		R2D.pushClip(bx + 18, by, bx + bw - 6, by + 18);
		R2D.text(shown + (caret ? "_" : ""), bx + 19, by + 5, query.isEmpty() && !searchFocused ? Theme.textFaint() : Theme.text());
		R2D.popClip();
		hits.add(new Hit(bx, by, bx + bw, by + 18, (x, y, b) -> {
			searchFocused = true;
			focusedText = null;
			screen.setTextInput(true);
		}));
	}

	private List<Module> visibleModules() {
		List<Module> out = new ArrayList<>();
		if (!query.isEmpty()) {
			String q = query.toLowerCase();
			for (Module m : ModuleManager.all()) {
				if (m.name().toLowerCase().contains(q) || m.listName().toLowerCase().contains(q)
						|| m.description().toLowerCase().contains(q)) out.add(m);
			}
			return out;
		}
		return ModuleManager.byCategory(category);
	}

	// ---- module list ------------------------------------------------------------------------------

	private void drawModuleList(int mx, int my) {
		List<Module> list = visibleModules();
		float itemH = 30, gap = 4;
		listContent = list.size() * (itemH + gap);
		float maxScroll = Math.max(0, listContent - listH);
		listScrollTarget = MathUtil.clamp(listScrollTarget, 0, maxScroll);
		float ca = contentAnim.get();
		R2D.pushClip(listX - 2, listY, listX + listW + 2, listY + listH);
		float y = listY - listScroll + (1f - ca) * 12f;
		for (int i = 0; i < list.size(); i++) {
			final Module m = list.get(i);
			float iy = y + i * (itemH + gap);
			if (iy + itemH < listY || iy > listY + listH) continue;
			boolean hov = in(mx, my, listX, Math.max(iy, listY), listX + listW, Math.min(iy + itemH, listY + listH));
			Anim h = anim(hover, m, 150);
			h.to(hov ? 1f : 0f);
			boolean sel = m == selected;
			int bg = sel ? Theme.panel3() : ColorUtil.lerp(Theme.panel(), Theme.panel2(), h.get());
			R2D.roundRect(listX, iy, listW, itemH, 8, bg);
			if (sel) R2D.roundOutline(listX, iy, listW, itemH, 8, 1f, ColorUtil.withAlpha(Theme.accent1, 170));
			if (m.isEnabled()) {
				R2D.roundRect(listX + 4, iy + 8, 2.5f, itemH - 16, 1.25f, Theme.gradient(i * 0.1f));
			}
			float nameScale = 1f;
			R2D.text(R2D.ellipsize(m.name(), listW - 46, nameScale), listX + 11, iy + 6, m.isEnabled() ? Theme.text() : Theme.textDim(), false, nameScale);
			R2D.text(R2D.ellipsize(m.description(), listW - 46, 0.72f), listX + 11, iy + 18, Theme.textFaint(), false, 0.72f);
			boolean toggleable = !(m instanceof ClickGuiModule);
			if (toggleable) {
				Anim t = anim(toggles, m, 180);
				t.to(m.isEnabled() ? 1f : 0f);
				Icons.toggle(listX + listW - 30, iy + itemH / 2f - 5.5f, 22, 11, t.get(), Theme.accent1, ColorUtil.withAlpha(0x808090, 120));
				hits.add(new Hit(listX + listW - 34, Math.max(iy, listY), listX + listW, Math.min(iy + itemH, listY + listH), (x, yy, b) -> m.toggle()));
			} else {
				Icons.draw("C", listX + listW - 19, iy + itemH / 2f, 9, Theme.textDim());
			}
			if (m.key.bound()) {
				String k = Keys.name(m.key.get());
				float kw = R2D.width(k) * 0.7f + 6;
				float kx = listX + listW - 38 - kw;
				R2D.roundRect(kx, iy + 4, kw, 9, 3, Theme.stroke());
				R2D.text(k, kx + 3, iy + 5.5f, Theme.textDim(), false, 0.7f);
			}
			hits.add(new Hit(listX, Math.max(iy, listY), listX + listW - 34, Math.min(iy + itemH, listY + listH), (x, yy, b) -> {
				if (b == BTN_RIGHT && !(m instanceof ClickGuiModule)) m.toggle();
				else selectModule(m);
			}));
		}
		if (list.isEmpty()) {
			String s = Lang.t("Nothing found", "Ничего не найдено");
			R2D.text(s, listX + listW / 2f - R2D.width(s) / 2f, listY + 20, Theme.textFaint());
		}
		R2D.popClip();
		scrollbar(listX + listW + 3, listY, listH, listScroll, listContent);
	}

	private void scrollbar(float x, float y, float h, float scroll, float content) {
		if (content <= h) return;
		float barH = Math.max(16, h * h / content);
		float by = y + (h - barH) * (scroll / (content - h));
		R2D.roundRect(x, by, 2, barH, 1, ColorUtil.withAlpha(Theme.accent1, 140));
	}

	private void selectModule(Module m) {
		if (selected == m) return;
		selected = m;
		setScrollTarget = 0;
		setScroll = 0;
		openColor = null;
		listeningKey = null;
		focusedText = null;
	}

	// ---- settings pane ----------------------------------------------------------------------------

	private void drawSettings(int mx, int my) {
		R2D.roundRect(setX, setY, setW, setH, 9, Theme.panel());
		if (selected == null) return;
		final Module m = selected;
		float x = setX + 10, w = setW - 20;
		R2D.pushClip(setX, setY, setX + setW, setY + setH);
		float y = setY + 10 - setScroll;
		float top = y;

		// header
		R2D.text(m.name(), x, y, Theme.text(), false, 1.25f);
		y += 14;
		for (String line : wrap(m.description(), w, 0.8f)) {
			R2D.text(line, x, y, Theme.textDim(), false, 0.8f);
			y += 9;
		}
		y += 4;
		boolean toggleable = !(m instanceof ClickGuiModule);
		if (toggleable) {
			// enable switch
			R2D.roundRect(x, y, w, 22, 7, Theme.panel2());
			R2D.text(m.isEnabled() ? Lang.t("Enabled", "Включён") : Lang.t("Disabled", "Выключен"), x + 8, y + 7, m.isEnabled() ? Theme.text() : Theme.textDim());
			Anim t = anim(toggles, m, 180);
			t.to(m.isEnabled() ? 1f : 0f);
			Icons.toggle(x + w - 30, y + 5.5f, 22, 11, t.get(), Theme.accent1, ColorUtil.withAlpha(0x808090, 120));
			hits.add(new Hit(x, Math.max(y, setY), x + w, Math.min(y + 22, setY + setH), (xx, yy, b) -> m.toggle()));
			y += 26;
		}
		// keybind row
		y = keyRow(m.key, toggleable ? Lang.t("Toggle key", "Клавиша вкл/выкл") : Lang.t("Menu key", "Клавиша меню"), x, y, w, mx, my);
		y += 4;
		R2D.rect(x, y, x + w, y + 1, Theme.stroke());
		y += 7;

		for (Setting<?> s : m.settings()) {
			if (!s.isVisible()) continue;
			if (s instanceof BoolSetting) y = boolRow((BoolSetting) s, x, y, w, mx, my);
			else if (s instanceof NumberSetting) y = numberRow((NumberSetting) s, x, y, w, mx, my);
			else if (s instanceof ModeSetting) y = modeRow((ModeSetting) s, x, y, w, mx, my);
			else if (s instanceof ColorSetting) y = colorRow((ColorSetting) s, x, y, w, mx, my);
			else if (s instanceof KeySetting) y = keyRow((KeySetting) s, s.name(), x, y, w, mx, my);
			else if (s instanceof TextSetting) y = textRow((TextSetting) s, x, y, w, mx, my);
			y += 3;
		}
		setContent = y + setScroll - top + 8;
		float maxScroll = Math.max(0, setContent - setH);
		setScrollTarget = MathUtil.clamp(setScrollTarget, 0, maxScroll);
		R2D.popClip();
		scrollbar(setX + setW - 4, setY + 4, setH - 8, setScroll, setContent);
	}

	private boolean visibleRow(float y, float h) {
		return y + h > setY && y < setY + setH;
	}

	private void addRowHit(float x1, float y1, float x2, float y2, Click c) {
		float a = Math.max(y1, setY), b = Math.min(y2, setY + setH);
		if (b > a) hits.add(new Hit(x1, a, x2, b, c));
	}

	private float boolRow(final BoolSetting s, float x, float y, float w, int mx, int my) {
		float h = 18;
		if (visibleRow(y, h)) {
			R2D.text(s.name(), x, y + 5, Theme.text());
			Anim t = anim(toggles, s, 160);
			t.to(s.on() ? 1f : 0f);
			Icons.toggle(x + w - 22, y + 3.5f, 22, 11, t.get(), Theme.accent1, ColorUtil.withAlpha(0x808090, 120));
			addRowHit(x, y, x + w, y + h, (xx, yy, b) -> {
				s.toggle();
				Config.markDirty();
			});
		}
		return y + h;
	}

	private float numberRow(final NumberSetting s, final float x, float y, final float w, int mx, int my) {
		float h = 26;
		if (visibleRow(y, h)) {
			R2D.text(s.name(), x, y + 2, Theme.text());
			String v = s.display();
			float vw = R2D.width(v) + 8;
			R2D.roundRect(x + w - vw, y, vw, 11, 4, Theme.panel3());
			R2D.text(v, x + w - vw + 4, y + 2, Theme.text(), false, 1f);
			float ty = y + 17;
			Anim fa = anim(hover, s, 120);
			fa.to((float) s.fraction());
			if (draggingSlider == s) fa.set((float) s.fraction());
			float f = fa.get();
			R2D.roundRect(x, ty, w, 3.5f, 1.75f, Theme.panel3());
			if (f > 0.005f) R2D.roundRectGradient(x, ty, w * f, 3.5f, 1.75f, Theme.accent1, Theme.accent2, true);
			float kx = x + w * f;
			R2D.circle(kx, ty + 1.75f, 4.5f, ColorUtil.withAlpha(Theme.accent2, 70));
			R2D.circle(kx, ty + 1.75f, 3.2f, 0xFFFFFFFF);
			addRowHit(x - 3, ty - 5, x + w + 3, ty + 8, (xx, yy, b) -> {
				draggingSlider = s;
				sliderX = x;
				sliderW = w;
				updateSlider(xx);
			});
		}
		return y + h;
	}

	private void updateSlider(double mx) {
		if (draggingSlider == null || sliderW <= 0) return;
		draggingSlider.setFraction((mx - sliderX) / sliderW);
		Config.markDirty();
	}

	private float modeRow(final ModeSetting s, float x, float y, float w, int mx, int my) {
		float labelH = 12;
		float chipH = 14, gap = 4;
		// lay out chips into rows
		List<float[]> pos = new ArrayList<>();
		float cx = x, cy = y + labelH;
		for (int i = 0; i < s.count(); i++) {
			float cw = R2D.width(s.nameAt(i)) * 0.85f + 12;
			if (cx + cw > x + w && cx > x) {
				cx = x;
				cy += chipH + gap;
			}
			pos.add(new float[]{cx, cy, cw});
			cx += cw + gap;
		}
		float h = cy + chipH - y;
		if (visibleRow(y, h)) {
			R2D.text(s.name(), x, y + 1, Theme.text());
			for (int i = 0; i < s.count(); i++) {
				final int idx = i;
				float[] p = pos.get(i);
				boolean sel = s.index() == i;
				boolean hov = in(mx, my, p[0], p[1], p[0] + p[2], p[1] + chipH);
				if (sel) R2D.roundRectGradient(p[0], p[1], p[2], chipH, 6, Theme.accent1, Theme.accent2, true);
				else R2D.roundRect(p[0], p[1], p[2], chipH, 6, hov ? Theme.panel3() : Theme.panel2());
				R2D.text(s.nameAt(i), p[0] + 6, p[1] + 4, sel ? 0xFFFFFFFF : Theme.textDim(), false, 0.85f);
				addRowHit(p[0], p[1], p[0] + p[2], p[1] + chipH, (xx, yy, b) -> {
					s.set(s.idAt(idx));
					Config.markDirty();
				});
			}
		}
		return y + h;
	}

	private float colorRow(final ColorSetting s, float x, float y, float w, int mx, int my) {
		float h = 18;
		if (visibleRow(y, h)) {
			R2D.text(s.name(), x, y + 5, Theme.text());
			final float sx = x + w - 26, sy = y + 2;
			R2D.roundRect(sx - 1, sy - 1, 28, 16, 5, Theme.stroke());
			R2D.roundRect4(sx, sy, 26, 14, 4, s.color(0f), s.color(0.25f), s.color(0.5f), s.color(0.75f));
			String tag = s.rainbow() ? Lang.t("RGB", "RGB") : (s.sync() ? Lang.t("Theme", "Тема") : "");
			if (!tag.isEmpty()) R2D.text(tag, sx - 4 - R2D.width(tag) * 0.75f, y + 6, Theme.textFaint(), false, 0.75f);
			addRowHit(sx - 2, sy - 2, sx + 28, sy + 16, (xx, yy, b) -> {
				if (openColor == s) {
					openColor = null;
					return;
				}
				openColor = s;
				pickerHsv = ColorUtil.toHsv(s.get());
				pickerX = Math.min(sx + 28 - 130, Mc.guiWidth() - 134f);
				pickerY = sy + 18;
				if (pickerY + 128 > Mc.guiHeight()) pickerY = sy - 130;
			});
		}
		return y + h;
	}

	private float keyRow(final KeySetting s, String label, float x, float y, float w, int mx, int my) {
		float h = 18;
		if (visibleRow(y, h)) {
			R2D.text(label, x, y + 5, Theme.text());
			boolean listening = listeningKey == s;
			String k = listening ? Lang.t("Press a key...", "Нажмите клавишу...") : (s.bound() ? Keys.name(s.get()) : Lang.t("None", "Нет"));
			float kw = R2D.width(k) + 14;
			float kx = x + w - kw;
			boolean hov = in(mx, my, kx, y + 2, kx + kw, y + 16);
			if (listening) R2D.roundRectGradient(kx, y + 2, kw, 14, 5, Theme.accent1, Theme.accent2, true);
			else R2D.roundRect(kx, y + 2, kw, 14, 5, hov ? Theme.panel3() : Theme.panel2());
			R2D.text(k, kx + 7, y + 5, listening ? 0xFFFFFFFF : Theme.textDim());
			addRowHit(kx, y + 2, kx + kw, y + 16, (xx, yy, b) -> {
				if (b == BTN_RIGHT) {
					s.set(KeySetting.NONE);
					Config.markDirty();
					return;
				}
				listeningKey = listening ? null : s;
			});
		}
		return y + h;
	}

	private float textRow(final TextSetting s, float x, float y, float w, int mx, int my) {
		float h = 30;
		if (visibleRow(y, h)) {
			R2D.text(s.name(), x, y + 1, Theme.text());
			boolean foc = focusedText == s;
			R2D.roundRect(x, y + 12, w, 16, 6, foc ? Theme.panel3() : Theme.panel2());
			if (foc) R2D.roundOutline(x, y + 12, w, 16, 6, 1f, ColorUtil.withAlpha(Theme.accent1, 200));
			boolean caret = foc && (System.currentTimeMillis() / 500) % 2 == 0;
			R2D.pushClip(x + 4, y + 12, x + w - 4, y + 28);
			R2D.text(s.get() + (caret ? "_" : ""), x + 6, y + 16, Theme.text());
			R2D.popClip();
			addRowHit(x, y + 12, x + w, y + 28, (xx, yy, b) -> {
				focusedText = s;
				searchFocused = false;
				screen.setTextInput(true);
			});
		}
		return y + h;
	}

	// ---- color picker -----------------------------------------------------------------------------

	private void drawColorPicker(int mx, int my) {
		final ColorSetting s = openColor;
		float x = pickerX, y = pickerY, w = 130, h = 128;
		R2D.shadow(x, y, w, h, 8, 12, ColorUtil.withAlpha(0, 150));
		R2D.roundRect(x, y, w, h, 8, Theme.panel());
		R2D.roundOutline(x, y, w, h, 8, 1f, Theme.stroke());
		float px = x + 8, py = y + 8, pw = w - 16, ph = 60;
		int hue = ColorUtil.hsv(pickerHsv[0], 1f, 1f);
		R2D.rect4(px, py, px + pw, py + ph, 0xFFFFFFFF, hue, 0xFF000000, 0xFF000000);
		R2D.gradientV(px, py, px + pw, py + ph, 0x00000000, 0xFF000000);
		R2D.ring(px + pw * pickerHsv[1], py + ph * (1f - pickerHsv[2]), 3f, 1.2f, 0xFFFFFFFF);
		// hue bar
		float hy = py + ph + 6;
		for (int i = 0; i < 6; i++) {
			float x1 = px + pw * i / 6f, x2 = px + pw * (i + 1) / 6f;
			R2D.gradientH(x1, hy, x2, hy + 6, ColorUtil.hsv(i / 6f, 1, 1), ColorUtil.hsv((i + 1) / 6f, 1, 1));
		}
		R2D.roundRect(px + pw * pickerHsv[0] - 1.5f, hy - 1, 3, 8, 1.5f, 0xFFFFFFFF);
		// alpha bar
		float ay = hy + 11;
		int alpha = ColorUtil.alpha(s.get());
		for (int i = 0; i < 16; i++) {
			R2D.rect(px + pw * i / 16f, ay, px + pw * (i + 1) / 16f, ay + 6, (i % 2 == 0) ? 0xFF9A9A9A : 0xFFDADADA);
		}
		int rgb = ColorUtil.hsv(pickerHsv[0], pickerHsv[1], pickerHsv[2]);
		R2D.gradientH(px, ay, px + pw, ay + 6, ColorUtil.withAlpha(rgb, 0), ColorUtil.withAlpha(rgb, 255));
		R2D.roundRect(px + pw * alpha / 255f - 1.5f, ay - 1, 3, 8, 1.5f, 0xFFFFFFFF);
		// rainbow / theme chips
		float cy = ay + 12;
		float cw = (pw - 4) / 2f;
		chip(px, cy, cw, Lang.t("Rainbow", "Радуга"), s.rainbow(), mx, my, () -> {
			s.setRainbow(!s.rainbow());
			Config.markDirty();
		});
		chip(px + cw + 4, cy, cw, Lang.t("Theme", "Тема"), s.sync(), mx, my, () -> {
			s.setSync(!s.sync());
			Config.markDirty();
		});
	}

	private void chip(float x, float y, float w, String label, boolean on, int mx, int my, final Runnable action) {
		boolean hov = in(mx, my, x, y, x + w, y + 14);
		if (on) R2D.roundRectGradient(x, y, w, 14, 6, Theme.accent1, Theme.accent2, true);
		else R2D.roundRect(x, y, w, 14, 6, hov ? Theme.panel3() : Theme.panel2());
		R2D.text(label, x + w / 2f - R2D.width(label) * 0.85f / 2f, y + 4, on ? 0xFFFFFFFF : Theme.textDim(), false, 0.85f);
	}

	private boolean pickerClick(double mx, double my, boolean drag) {
		if (openColor == null) return false;
		float x = pickerX, y = pickerY, w = 130, h = 128;
		float px = x + 8, py = y + 8, pw = w - 16, ph = 60;
		float hy = py + ph + 6, ay = hy + 11, cy = ay + 12, cw = (pw - 4) / 2f;
		if (!drag) {
			if (!in(mx, my, x, y, x + w, y + h)) return false;
			if (in(mx, my, px, py, px + pw, py + ph)) pickerDrag = 1;
			else if (in(mx, my, px, hy - 2, px + pw, hy + 8)) pickerDrag = 2;
			else if (in(mx, my, px, ay - 2, px + pw, ay + 8)) pickerDrag = 3;
			else if (in(mx, my, px, cy, px + cw, cy + 14)) {
				openColor.setRainbow(!openColor.rainbow());
				Config.markDirty();
				return true;
			} else if (in(mx, my, px + cw + 4, cy, px + pw, cy + 14)) {
				openColor.setSync(!openColor.sync());
				Config.markDirty();
				return true;
			} else {
				return true;
			}
		}
		float fx = (float) MathUtil.clamp((mx - px) / pw, 0, 1);
		float fy = (float) MathUtil.clamp((my - py) / ph, 0, 1);
		int alpha = ColorUtil.alpha(openColor.get());
		if (pickerDrag == 1) {
			pickerHsv[1] = fx;
			pickerHsv[2] = 1f - fy;
		} else if (pickerDrag == 2) {
			pickerHsv[0] = Math.min(0.999f, fx);
		} else if (pickerDrag == 3) {
			alpha = Math.round(fx * 255);
		}
		if (pickerDrag != 0) {
			openColor.setRainbow(false);
			openColor.setSync(false);
			openColor.set(ColorUtil.withAlpha(ColorUtil.hsv(pickerHsv[0], pickerHsv[1], pickerHsv[2]), alpha));
			Config.markDirty();
		}
		return true;
	}

	// ---- input ------------------------------------------------------------------------------------

	private static boolean in(double mx, double my, float x1, float y1, float x2, float y2) {
		return mx >= x1 && mx <= x2 && my >= y1 && my <= y2;
	}

	@Override
	public boolean mouseClicked(double mx, double my, int button) {
		if (closing) return true;
		if (openColor != null) {
			if (pickerClick(mx, my, false)) return true;
			openColor = null;
		}
		if (listeningKey != null) {
			listeningKey = null;
			return true;
		}
		boolean wasTyping = searchFocused || focusedText != null;
		searchFocused = false;
		focusedText = null;
		for (int i = hits.size() - 1; i >= 0; i--) {
			Hit h = hits.get(i);
			if (h.contains(mx, my)) {
				h.click.run(mx, my, button);
				if (wasTyping && !searchFocused && focusedText == null) screen.setTextInput(false);
				return true;
			}
		}
		if (wasTyping) screen.setTextInput(false);
		// drag the window by its free areas (top of the settings column or the logo)
		if (in(mx, my, wx, wy, wx + ww, wy + wh)) {
			if (my < wy + 44 || in(mx, my, wx, wy, wx + sideW, wy + 46)) {
				draggingWindow = true;
				dragDx = (float) (mx - wx);
				dragDy = (float) (my - wy);
			}
			return true;
		}
		return false;
	}

	@Override
	public boolean mouseReleased(double mx, double my, int button) {
		draggingSlider = null;
		pickerDrag = 0;
		if (draggingWindow) {
			draggingWindow = false;
			Config.markDirty();
		}
		return true;
	}

	@Override
	public boolean mouseDragged(double mx, double my, int button, double dx, double dy) {
		if (pickerDrag != 0) {
			pickerClick(mx, my, true);
			return true;
		}
		if (draggingSlider != null) {
			updateSlider(mx);
			return true;
		}
		if (draggingWindow) {
			float nx = (float) mx - dragDx, ny = (float) my - dragDy;
			ClientSettings.menuX = (nx + ww / 2f) / Math.max(1, sw);
			ClientSettings.menuY = (ny + wh / 2f) / Math.max(1, sh);
			return true;
		}
		return false;
	}

	@Override
	public boolean mouseScrolled(double mx, double my, double amount) {
		if (in(mx, my, listX, listY, listX + listW, listY + listH)) {
			listScrollTarget -= (float) amount * 24f;
			return true;
		}
		if (in(mx, my, setX, setY, setX + setW, setY + setH)) {
			setScrollTarget -= (float) amount * 24f;
			return true;
		}
		return false;
	}

	@Override
	public boolean keyPressed(int key, int modifiers) {
		if (listeningKey != null) {
			if (key == Keys.ESCAPE) listeningKey = null;
			else {
				listeningKey.set(key == Keys.BACKSPACE || key == Keys.DELETE ? KeySetting.NONE : key);
				listeningKey = null;
				Config.markDirty();
			}
			return true;
		}
		if (searchFocused) {
			if (key == Keys.BACKSPACE && !query.isEmpty()) {
				query = query.substring(0, query.length() - 1);
				listScrollTarget = 0;
				return true;
			}
			if (key == Keys.ENTER || key == Keys.ESCAPE) {
				searchFocused = false;
				screen.setTextInput(false);
				return true;
			}
			return key != Keys.ESCAPE;
		}
		if (focusedText != null) {
			String v = focusedText.get();
			if (key == Keys.BACKSPACE && !v.isEmpty()) {
				focusedText.set(v.substring(0, v.length() - 1));
				Config.markDirty();
				return true;
			}
			if (key == Keys.ENTER || key == Keys.ESCAPE) {
				focusedText = null;
				screen.setTextInput(false);
				return true;
			}
			return true;
		}
		// typing anywhere starts a search
		return false;
	}

	@Override
	public boolean charTyped(String text) {
		if (text == null || text.isEmpty() || text.charAt(0) < 32) return false;
		if (focusedText != null) {
			focusedText.set(focusedText.get() + text);
			Config.markDirty();
			return true;
		}
		if (!searchFocused) {
			if (listeningKey != null) return true;
			searchFocused = true;
			screen.setTextInput(true);
		}
		if (query.length() < 24) {
			query += text;
			listScrollTarget = 0;
		}
		return true;
	}

	private static List<String> wrap(String text, float width, float scale) {
		List<String> lines = new ArrayList<>();
		StringBuilder line = new StringBuilder();
		for (String word : text.split(" ")) {
			String candidate = line.length() == 0 ? word : line + " " + word;
			if (R2D.width(candidate) * scale > width && line.length() > 0) {
				lines.add(line.toString());
				line = new StringBuilder(word);
			} else {
				line = new StringBuilder(candidate);
			}
		}
		if (line.length() > 0) lines.add(line.toString());
		return lines;
	}
}
