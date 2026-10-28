package dev.rteam.visuals.gui.screen;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.config.Config;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

import java.util.ArrayList;
import java.util.List;

/** Drag HUD elements around; right click toggles, mouse wheel scales. */
public final class HudEditor implements View {
	private static boolean open;

	private RvScreen screen;
	private final RvScreen parent;
	private HudModule dragging;
	private float dx, dy;
	private float guideX = -1, guideY = -1;
	private final Anim fade = new Anim(0f, 200, Anim.Easing.OUT_CUBIC);

	private HudEditor(RvScreen parent) {
		this.parent = parent;
	}

	public static RvScreen create(RvScreen parent) {
		HudEditor e = new HudEditor(parent);
		RvScreen s = new RvScreen(e);
		e.screen = s;
		return s;
	}

	public static boolean isOpen() {
		return open;
	}

	private List<HudModule> huds() {
		List<HudModule> out = new ArrayList<>();
		for (Module m : ModuleManager.all()) if (m instanceof HudModule) out.add((HudModule) m);
		return out;
	}

	@Override
	public void init(int width, int height) {
		open = true;
		fade.to(1f);
	}

	@Override
	public void render(int mouseX, int mouseY, float delta) {
		float sw = Mc.guiWidth(), sh = Mc.guiHeight();
		R2D.setAlpha(fade.get());
		R2D.rect(0, 0, sw, sh, ColorUtil.withAlpha(0x000000, 70));
		// center lines
		R2D.rect(sw / 2f - 0.5f, 0, sw / 2f + 0.5f, sh, ColorUtil.withAlpha(0xFFFFFF, 25));
		R2D.rect(0, sh / 2f - 0.5f, sw, sh / 2f + 0.5f, ColorUtil.withAlpha(0xFFFFFF, 25));
		if (guideX >= 0) R2D.rect(guideX - 0.5f, 0, guideX + 0.5f, sh, ColorUtil.withAlpha(Theme.accent1, 200));
		if (guideY >= 0) R2D.rect(0, guideY - 0.5f, sw, guideY + 0.5f, ColorUtil.withAlpha(Theme.accent1, 200));

		for (HudModule h : huds()) {
			R2D.setAlpha(fade.get() * (h.isEnabled() ? 1f : 0.35f));
			h.renderAt(delta, true);
			float x = h.x(), y = h.y(), w = h.width(), hh = h.height();
			boolean hov = mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + hh;
			int c = h == dragging ? Theme.accent1 : (hov ? ColorUtil.withAlpha(Theme.accent2, 220) : ColorUtil.withAlpha(0xFFFFFF, 60));
			R2D.roundOutline(x - 2, y - 2, w + 4, hh + 4, 4, 1f, c);
			if (hov || h == dragging) {
				String label = h.name() + (h.isEnabled() ? "" : " (" + Lang.t("off", "выкл") + ")");
				float ly = y - 12 < 2 ? y + hh + 4 : y - 12;
				R2D.roundRect(x - 2, ly, R2D.width(label) * 0.8f + 8, 10, 3, Theme.panel());
				R2D.text(label, x + 2, ly + 2, Theme.text(), false, 0.8f);
			}
		}
		R2D.setAlpha(fade.get());
		// hint bar and done button
		String hint = Lang.t("Drag elements  ·  Right click: on/off  ·  Wheel: size  ·  Esc: back",
				"Перетаскивайте элементы  ·  ПКМ: вкл/выкл  ·  Колесо: размер  ·  Esc: назад");
		float hw = R2D.width(hint) * 0.85f + 16;
		R2D.roundRect(sw / 2f - hw / 2f, 6, hw, 14, 7, Theme.panel());
		R2D.text(hint, sw / 2f - hw / 2f + 8, 9.5f, Theme.textDim(), false, 0.85f);
		float bw = 70, bx = sw / 2f - bw / 2f, by = sh - 26;
		boolean hov = mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + 18;
		R2D.roundRectGradient(bx, by, bw, 18, 9, hov ? Theme.accent2 : Theme.accent1, hov ? Theme.accent1 : Theme.accent2, true);
		String done = Lang.t("Done", "Готово");
		R2D.text(done, sw / 2f - R2D.width(done) / 2f, by + 5, 0xFFFFFFFF);
		R2D.setAlpha(1f);
	}

	@Override
	public boolean mouseClicked(double mx, double my, int button) {
		float sw = Mc.guiWidth(), sh = Mc.guiHeight();
		float bw = 70, bx = sw / 2f - bw / 2f, by = sh - 26;
		if (mx >= bx && mx <= bx + bw && my >= by && my <= by + 18) {
			back();
			return true;
		}
		List<HudModule> list = huds();
		for (int i = list.size() - 1; i >= 0; i--) {
			HudModule h = list.get(i);
			if (!h.contains(mx, my)) continue;
			if (button == 1) {
				h.toggle();
			} else {
				dragging = h;
				dx = (float) (mx - h.x());
				dy = (float) (my - h.y());
			}
			return true;
		}
		return false;
	}

	@Override
	public boolean mouseReleased(double mx, double my, int button) {
		if (dragging != null) Config.markDirty();
		dragging = null;
		guideX = guideY = -1;
		return true;
	}

	@Override
	public boolean mouseDragged(double mx, double my, int button, double ddx, double ddy) {
		if (dragging == null) return false;
		float sw = Mc.guiWidth(), sh = Mc.guiHeight();
		float x = (float) mx - dx, y = (float) my - dy;
		float w = dragging.width(), h = dragging.height();
		float snap = 4f;
		guideX = guideY = -1;
		// snap to screen edges and center lines
		if (Math.abs(x + w / 2f - sw / 2f) < snap) {
			x = sw / 2f - w / 2f;
			guideX = sw / 2f;
		} else if (Math.abs(x - 2) < snap) {
			x = 2;
		} else if (Math.abs(x + w - (sw - 2)) < snap) {
			x = sw - 2 - w;
		}
		if (Math.abs(y + h / 2f - sh / 2f) < snap) {
			y = sh / 2f - h / 2f;
			guideY = sh / 2f;
		} else if (Math.abs(y - 2) < snap) {
			y = 2;
		} else if (Math.abs(y + h - (sh - 2)) < snap) {
			y = sh - 2 - h;
		}
		dragging.moveTo(x, y);
		return true;
	}

	@Override
	public boolean mouseScrolled(double mx, double my, double amount) {
		for (HudModule h : huds()) {
			if (h.contains(mx, my)) {
				h.scale.set(MathUtil.clamp(h.scale.d() + amount * 0.05, h.scale.min(), h.scale.max()));
				Config.markDirty();
				return true;
			}
		}
		return false;
	}

	@Override
	public boolean keyPressed(int key, int modifiers) {
		return false;
	}

	@Override
	public boolean charTyped(String text) {
		return false;
	}

	private void back() {
		open = false;
		Mc.setScreen(parent != null ? parent : null);
	}

	@Override
	public boolean requestClose() {
		back();
		return false;
	}

	@Override
	public void removed() {
		open = false;
		Config.save();
	}

	@Override
	public void tick() {
	}

	@Override
	public boolean vanillaBackground() {
		return false;
	}
}
