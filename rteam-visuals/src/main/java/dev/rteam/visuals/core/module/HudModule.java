package dev.rteam.visuals.core.module;

import com.google.gson.JsonObject;
import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

/**
 * A HUD element that can be dragged in the HUD editor.
 * The position is stored relative to the screen, anchored to the nearest edge,
 * so elements keep their place on any resolution and GUI scale.
 */
public abstract class HudModule extends Module {
	/** Anchor point of the element in 0..1 screen coordinates. */
	private float px;
	private float py;
	protected float width = 40;
	protected float height = 12;
	public final NumberSetting scale;

	protected HudModule(String id, Lang name, Lang description, float defaultX, float defaultY) {
		super(id, name, description, Category.HUD);
		this.px = defaultX;
		this.py = defaultY;
		this.scale = add(new NumberSetting("scale", Lang.of("Scale", "Размер"), 1.0, 0.5, 2.0, 0.05));
	}

	@Override
	public boolean showInList() {
		return false;
	}

	/** Draws the element with its top-left corner at (0, 0); must set width/height. */
	protected abstract void draw(float partialTick, boolean editor);

	/** Whether there is something to show right now (outside the HUD editor). */
	protected boolean visible() {
		return true;
	}

	@Override
	public void onRender2D(float partialTick) {
		if (dev.rteam.visuals.gui.screen.HudEditor.isOpen() || !visible()) return;
		renderAt(partialTick, false);
	}

	public void renderAt(float partialTick, boolean editor) {
		dev.rteam.visuals.render.R2D.push();
		dev.rteam.visuals.render.R2D.translate(x(), y());
		dev.rteam.visuals.render.R2D.scale(scale.f());
		try {
			draw(partialTick, editor);
		} finally {
			dev.rteam.visuals.render.R2D.pop();
		}
	}

	public float width() {
		return width * scale.f();
	}

	public float height() {
		return height * scale.f();
	}

	private static float anchor(float p) {
		return p < 0.34f ? 0f : (p > 0.66f ? 1f : 0.5f);
	}

	/** True when the element sits on the right side of the screen (lists should grow leftwards). */
	public boolean rightAligned() {
		return anchor(px) == 1f;
	}

	public boolean bottomAligned() {
		return anchor(py) == 1f;
	}

	public float x() {
		float sw = Mc.guiWidth();
		float x = px * sw - width() * anchor(px);
		return MathUtil.clamp(x, 0f, Math.max(0f, sw - width()));
	}

	public float y() {
		float sh = Mc.guiHeight();
		float y = py * sh - height() * anchor(py);
		return MathUtil.clamp(y, 0f, Math.max(0f, sh - height()));
	}

	/** Moves the element so its top-left corner is at (x, y). */
	public void moveTo(float x, float y) {
		float sw = Math.max(1f, Mc.guiWidth());
		float sh = Math.max(1f, Mc.guiHeight());
		x = MathUtil.clamp(x, 0f, Math.max(0f, sw - width()));
		y = MathUtil.clamp(y, 0f, Math.max(0f, sh - height()));
		// pick the anchor from the element's center, then store the anchor point
		float cx = (x + width() / 2f) / sw;
		float cy = (y + height() / 2f) / sh;
		float ax = anchor(cx);
		float ay = anchor(cy);
		px = (x + width() * ax) / sw;
		py = (y + height() * ay) / sh;
	}

	public boolean contains(double mx, double my) {
		float x = x(), y = y();
		return mx >= x && mx <= x + width() && my >= y && my <= y + height();
	}

	public void savePosition(JsonObject o) {
		o.addProperty("x", px);
		o.addProperty("y", py);
	}

	public void loadPosition(JsonObject o) {
		if (o.has("x")) px = MathUtil.clamp(o.get("x").getAsFloat(), 0f, 1f);
		if (o.has("y")) py = MathUtil.clamp(o.get("y").getAsFloat(), 0f, 1f);
	}
}
