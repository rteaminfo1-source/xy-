package dev.rteam.visuals.render;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.util.ColorUtil;
import net.minecraft.world.item.ItemStack;

import java.util.ArrayDeque;
import java.util.Deque;

/**
 * 2D drawing for the HUD and the menu, the same on every Minecraft version.
 * Shapes are turned into anti-aliased colored quads (an extra 1 px band fading to
 * transparent), batched and handed to {@link Backend2D}, which talks to the game.
 * Coordinates are GUI units (floats); {@link #push()}/{@link #translate}/{@link #scale}
 * work on R2D's own transform.
 */
public final class R2D {
	private R2D() {
	}

	private static Object context;
	private static final QuadBuffer QUADS = new QuadBuffer();
	private static float tx, ty, sx = 1f, sy = 1f;
	private static final float[] STACK = new float[4 * 64];
	private static int sp;
	private static final Deque<float[]> CLIPS = new ArrayDeque<>();
	private static float feather = 0.5f;
	private static float pixel = 0.5f;
	private static float globalAlpha = 1f;

	// ---- frame ----------------------------------------------------------------------------------

	/** Starts drawing on a vanilla graphics object (PoseStack / GuiGraphics / GuiGraphicsExtractor). */
	public static void begin(Object graphics) {
		context = graphics;
		tx = ty = 0f;
		sx = sy = 1f;
		sp = 0;
		CLIPS.clear();
		QUADS.clear();
		globalAlpha = 1f;
		pixel = (float) (1.0 / Math.max(1.0, Mc.guiScale()));
		feather = pixel;
		Backend2D.begin(graphics);
	}

	public static void end() {
		flush();
		while (!CLIPS.isEmpty()) popClip();
		Backend2D.end(context);
		context = null;
	}

	public static boolean active() {
		return context != null;
	}

	public static Object context() {
		return context;
	}

	/** Draws everything recorded so far. Called automatically before text and items. */
	public static void flush() {
		if (context == null || QUADS.isEmpty()) {
			QUADS.clear();
			return;
		}
		Backend2D.drawQuads(context, QUADS, currentClip());
		QUADS.clear();
	}

	/** Content drawn after this goes on top of everything before (popups, tooltips). */
	public static void layer() {
		flush();
		Backend2D.nextLayer(context);
	}

	/** Size of one physical pixel in GUI units. */
	public static float pixel() {
		return pixel;
	}

	/** Multiplies the alpha of everything drawn (for fade animations). */
	public static void setAlpha(float a) {
		globalAlpha = Math.max(0f, Math.min(1f, a));
	}

	public static float alpha() {
		return globalAlpha;
	}

	private static int a(int c) {
		return globalAlpha >= 1f ? c : ColorUtil.fade(c, globalAlpha);
	}

	// ---- transform ------------------------------------------------------------------------------

	public static void push() {
		if (sp + 4 > STACK.length) throw new IllegalStateException("R2D stack overflow");
		STACK[sp++] = tx;
		STACK[sp++] = ty;
		STACK[sp++] = sx;
		STACK[sp++] = sy;
	}

	public static void pop() {
		if (sp < 4) return;
		sy = STACK[--sp];
		sx = STACK[--sp];
		ty = STACK[--sp];
		tx = STACK[--sp];
	}

	public static void translate(float x, float y) {
		tx += x * sx;
		ty += y * sy;
	}

	public static void scale(float s) {
		sx *= s;
		sy *= s;
	}

	/** Scales around a point (for pop-in animations). */
	public static void scaleAround(float s, float cx, float cy) {
		translate(cx, cy);
		scale(s);
		translate(-cx, -cy);
	}

	private static float X(float x) {
		return tx + x * sx;
	}

	private static float Y(float y) {
		return ty + y * sy;
	}

	// ---- clipping -------------------------------------------------------------------------------

	public static void pushClip(float x1, float y1, float x2, float y2) {
		flush();
		float[] r = {X(x1), Y(y1), X(x2), Y(y2)};
		float[] parent = currentClip();
		if (parent != null) {
			r[0] = Math.max(r[0], parent[0]);
			r[1] = Math.max(r[1], parent[1]);
			r[2] = Math.min(r[2], parent[2]);
			r[3] = Math.min(r[3], parent[3]);
		}
		if (r[2] < r[0]) r[2] = r[0];
		if (r[3] < r[1]) r[3] = r[1];
		CLIPS.push(r);
		Backend2D.pushClip(context, r);
	}

	public static void popClip() {
		flush();
		if (CLIPS.isEmpty()) return;
		CLIPS.pop();
		Backend2D.popClip(context, currentClip());
	}

	private static float[] currentClip() {
		return CLIPS.peek();
	}

	// ---- raw quads ------------------------------------------------------------------------------

	public static void quad(float x0, float y0, int c0, float x1, float y1, int c1,
							float x2, float y2, int c2, float x3, float y3, int c3) {
		// keep the winding of vanilla quads (top-left, bottom-left, bottom-right, top-right)
		// so nothing gets culled on versions where the GUI pipeline culls back faces
		float cross = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
		if (cross > 0f) {
			QUADS.add(X(x0), Y(y0), 0f, a(c0), X(x3), Y(y3), 0f, a(c3), X(x2), Y(y2), 0f, a(c2), X(x1), Y(y1), 0f, a(c1));
		} else {
			QUADS.add(X(x0), Y(y0), 0f, a(c0), X(x1), Y(y1), 0f, a(c1), X(x2), Y(y2), 0f, a(c2), X(x3), Y(y3), 0f, a(c3));
		}
	}

	public static void triangle(float x0, float y0, int c0, float x1, float y1, int c1, float x2, float y2, int c2) {
		quad(x0, y0, c0, x1, y1, c1, x2, y2, c2, x2, y2, c2);
	}

	// ---- rectangles -----------------------------------------------------------------------------

	public static void rect(float x1, float y1, float x2, float y2, int color) {
		quad(x1, y1, color, x1, y2, color, x2, y2, color, x2, y1, color);
	}

	public static void rectWH(float x, float y, float w, float h, int color) {
		rect(x, y, x + w, y + h, color);
	}

	public static void gradientV(float x1, float y1, float x2, float y2, int top, int bottom) {
		quad(x1, y1, top, x1, y2, bottom, x2, y2, bottom, x2, y1, top);
	}

	public static void gradientH(float x1, float y1, float x2, float y2, int left, int right) {
		quad(x1, y1, left, x1, y2, left, x2, y2, right, x2, y1, right);
	}

	/** Four corner colors: top-left, top-right, bottom-right, bottom-left. */
	public static void rect4(float x1, float y1, float x2, float y2, int tl, int tr, int br, int bl) {
		quad(x1, y1, tl, x1, y2, bl, x2, y2, br, x2, y1, tr);
	}

	public static void outline(float x1, float y1, float x2, float y2, float t, int color) {
		rect(x1, y1, x2, y1 + t, color);
		rect(x1, y2 - t, x2, y2, color);
		rect(x1, y1 + t, x1 + t, y2 - t, color);
		rect(x2 - t, y1 + t, x2, y2 - t, color);
	}

	// ---- rounded shapes ---------------------------------------------------------------------------

	private static float[] px = new float[256];
	private static float[] py = new float[256];
	private static float[] nx = new float[256];
	private static float[] ny = new float[256];
	private static int[] pc = new int[256];
	private static int pn;

	private static void ensurePoints(int n) {
		if (px.length < n) {
			px = new float[n * 2];
			py = new float[n * 2];
			nx = new float[n * 2];
			ny = new float[n * 2];
			pc = new int[n * 2];
		}
	}

	private static int segments(float radius) {
		float r = radius * sx / pixel;
		int s = (int) Math.ceil(r / 2.5f);
		return Math.max(2, Math.min(16, s));
	}

	/** Outline points of a rounded rectangle, clockwise starting at the top-left corner. */
	private static void roundedPoints(float x, float y, float w, float h, float r) {
		r = Math.max(0f, Math.min(r, Math.min(w, h) / 2f));
		int seg = r <= 0f ? 1 : segments(r);
		ensurePoints((seg + 1) * 4);
		pn = 0;
		corner(x + r, y + r, r, 180f, 270f, seg);
		corner(x + w - r, y + r, r, 270f, 360f, seg);
		corner(x + w - r, y + h - r, r, 0f, 90f, seg);
		corner(x + r, y + h - r, r, 90f, 180f, seg);
	}

	private static void corner(float cx, float cy, float r, float from, float to, int seg) {
		for (int i = 0; i <= seg; i++) {
			double ang = Math.toRadians(from + (to - from) * i / seg);
			float c = (float) Math.cos(ang), s = (float) Math.sin(ang);
			px[pn] = cx + c * r;
			py[pn] = cy + s * r;
			nx[pn] = c;
			ny[pn] = s;
			pn++;
		}
	}

	/** Fills the polygon in px/py with a center fan and an anti-aliasing band outside. */
	private static void fillPoints(float cx, float cy, int centerColor, float aa) {
		for (int i = 0; i < pn; i++) {
			int j = (i + 1) % pn;
			triangle(cx, cy, centerColor, px[i], py[i], pc[i], px[j], py[j], pc[j]);
		}
		if (aa > 0f) {
			for (int i = 0; i < pn; i++) {
				int j = (i + 1) % pn;
				quad(px[i], py[i], pc[i],
						px[i] + nx[i] * aa, py[i] + ny[i] * aa, ColorUtil.withAlpha(pc[i], 0),
						px[j] + nx[j] * aa, py[j] + ny[j] * aa, ColorUtil.withAlpha(pc[j], 0),
						px[j], py[j], pc[j]);
			}
		}
	}

	public static void roundRect(float x, float y, float w, float h, float r, int color) {
		if (w <= 0 || h <= 0) return;
		roundedPoints(x, y, w, h, r);
		for (int i = 0; i < pn; i++) pc[i] = color;
		fillPoints(x + w / 2f, y + h / 2f, color, feather / sx);
	}

	/** Rounded rectangle with a gradient (horizontal or vertical). */
	public static void roundRectGradient(float x, float y, float w, float h, float r, int c1, int c2, boolean horizontal) {
		if (w <= 0 || h <= 0) return;
		roundedPoints(x, y, w, h, r);
		for (int i = 0; i < pn; i++) {
			float t = horizontal ? (px[i] - x) / w : (py[i] - y) / h;
			pc[i] = ColorUtil.lerp(c1, c2, t);
		}
		fillPoints(x + w / 2f, y + h / 2f, ColorUtil.lerp(c1, c2, 0.5f), feather / sx);
	}

	/** Rounded rectangle with four corner colors (smooth 2D gradient). */
	public static void roundRect4(float x, float y, float w, float h, float r, int tl, int tr, int br, int bl) {
		if (w <= 0 || h <= 0) return;
		roundedPoints(x, y, w, h, r);
		for (int i = 0; i < pn; i++) {
			float u = (px[i] - x) / w, v = (py[i] - y) / h;
			pc[i] = ColorUtil.lerp(ColorUtil.lerp(tl, tr, u), ColorUtil.lerp(bl, br, u), v);
		}
		int center = ColorUtil.lerp(ColorUtil.lerp(tl, tr, 0.5f), ColorUtil.lerp(bl, br, 0.5f), 0.5f);
		fillPoints(x + w / 2f, y + h / 2f, center, feather / sx);
	}

	public static void roundOutline(float x, float y, float w, float h, float r, float t, int color) {
		if (w <= 0 || h <= 0) return;
		roundedPoints(x, y, w, h, r);
		int n = pn;
		float[] ox = new float[n], oy = new float[n], onx = new float[n], ony = new float[n];
		System.arraycopy(px, 0, ox, 0, n);
		System.arraycopy(py, 0, oy, 0, n);
		System.arraycopy(nx, 0, onx, 0, n);
		System.arraycopy(ny, 0, ony, 0, n);
		float aa = feather / sx;
		int clear = ColorUtil.withAlpha(color, 0);
		for (int i = 0; i < n; i++) {
			int j = (i + 1) % n;
			// band from (p - n*t) to p
			float ix0 = ox[i] - onx[i] * t, iy0 = oy[i] - ony[i] * t;
			float ix1 = ox[j] - onx[j] * t, iy1 = oy[j] - ony[j] * t;
			quad(ix0, iy0, color, ox[i], oy[i], color, ox[j], oy[j], color, ix1, iy1, color);
			// outer and inner anti-aliasing
			quad(ox[i], oy[i], color, ox[i] + onx[i] * aa, oy[i] + ony[i] * aa, clear,
					ox[j] + onx[j] * aa, oy[j] + ony[j] * aa, clear, ox[j], oy[j], color);
			quad(ix0, iy0, color, ix1, iy1, color, ix1 - onx[j] * aa, iy1 - ony[j] * aa, clear,
					ix0 - onx[i] * aa, iy0 - ony[i] * aa, clear);
		}
	}

	/** Soft shadow / glow around a rounded rectangle. */
	public static void shadow(float x, float y, float w, float h, float r, float size, int color) {
		if (size <= 0f) return;
		roundedPoints(x, y, w, h, r);
		int clear = ColorUtil.withAlpha(color, 0);
		int mid = ColorUtil.fade(color, 0.35f);
		float half = size * 0.45f;
		for (int i = 0; i < pn; i++) {
			int j = (i + 1) % pn;
			// two bands give a softer, more natural falloff than one linear band
			quad(px[i], py[i], color, px[i] + nx[i] * half, py[i] + ny[i] * half, mid,
					px[j] + nx[j] * half, py[j] + ny[j] * half, mid, px[j], py[j], color);
			quad(px[i] + nx[i] * half, py[i] + ny[i] * half, mid, px[i] + nx[i] * size, py[i] + ny[i] * size, clear,
					px[j] + nx[j] * size, py[j] + ny[j] * size, clear, px[j] + nx[j] * half, py[j] + ny[j] * half, mid);
		}
	}

	// ---- circles --------------------------------------------------------------------------------

	private static int circleSegments(float r) {
		float rp = r * sx / pixel;
		return Math.max(12, Math.min(96, (int) (rp * 1.6f)));
	}

	public static void circle(float cx, float cy, float r, int color) {
		int seg = circleSegments(r);
		ensurePoints(seg);
		pn = seg;
		for (int i = 0; i < seg; i++) {
			double ang = Math.PI * 2 * i / seg;
			float c = (float) Math.cos(ang), s = (float) Math.sin(ang);
			px[i] = cx + c * r;
			py[i] = cy + s * r;
			nx[i] = c;
			ny[i] = s;
			pc[i] = color;
		}
		fillPoints(cx, cy, color, feather / sx);
	}

	/** Arc band between radius r - t and r, from start to end degrees (0 = right, clockwise). */
	public static void arc(float cx, float cy, float r, float t, float startDeg, float endDeg, int color) {
		arcGradient(cx, cy, r, t, startDeg, endDeg, color, color);
	}

	public static void arcGradient(float cx, float cy, float r, float t, float startDeg, float endDeg, int c1, int c2) {
		float sweep = endDeg - startDeg;
		if (Math.abs(sweep) < 0.01f) return;
		int seg = Math.max(2, (int) (circleSegments(r) * Math.abs(sweep) / 360f));
		float aa = feather / sx;
		float ri = r - t;
		for (int i = 0; i < seg; i++) {
			double a0 = Math.toRadians(startDeg + sweep * i / seg);
			double a1 = Math.toRadians(startDeg + sweep * (i + 1) / seg);
			float c0 = (float) Math.cos(a0), s0 = (float) Math.sin(a0);
			float c1x = (float) Math.cos(a1), s1 = (float) Math.sin(a1);
			int k0 = ColorUtil.lerp(c1, c2, i / (float) seg);
			int k1 = ColorUtil.lerp(c1, c2, (i + 1) / (float) seg);
			int z0 = ColorUtil.withAlpha(k0, 0), z1 = ColorUtil.withAlpha(k1, 0);
			quad(cx + c0 * ri, cy + s0 * ri, k0, cx + c0 * r, cy + s0 * r, k0, cx + c1x * r, cy + s1 * r, k1, cx + c1x * ri, cy + s1 * ri, k1);
			quad(cx + c0 * r, cy + s0 * r, k0, cx + c0 * (r + aa), cy + s0 * (r + aa), z0, cx + c1x * (r + aa), cy + s1 * (r + aa), z1, cx + c1x * r, cy + s1 * r, k1);
			quad(cx + c0 * (ri - aa), cy + s0 * (ri - aa), z0, cx + c0 * ri, cy + s0 * ri, k0, cx + c1x * ri, cy + s1 * ri, k1, cx + c1x * (ri - aa), cy + s1 * (ri - aa), z1);
		}
	}

	public static void ring(float cx, float cy, float r, float t, int color) {
		arc(cx, cy, r, t, 0f, 360f, color);
	}

	/** Thick line with rounded look (anti-aliased sides). */
	public static void line(float x1, float y1, float x2, float y2, float width, int color) {
		float dx = x2 - x1, dy = y2 - y1;
		float len = (float) Math.sqrt(dx * dx + dy * dy);
		if (len < 1e-4f) return;
		float nxl = -dy / len, nyl = dx / len;
		float hw = width / 2f;
		float aa = feather / sx;
		int clear = ColorUtil.withAlpha(color, 0);
		quad(x1 + nxl * hw, y1 + nyl * hw, color, x1 - nxl * hw, y1 - nyl * hw, color,
				x2 - nxl * hw, y2 - nyl * hw, color, x2 + nxl * hw, y2 + nyl * hw, color);
		quad(x1 + nxl * hw, y1 + nyl * hw, color, x2 + nxl * hw, y2 + nyl * hw, color,
				x2 + nxl * (hw + aa), y2 + nyl * (hw + aa), clear, x1 + nxl * (hw + aa), y1 + nyl * (hw + aa), clear);
		quad(x1 - nxl * hw, y1 - nyl * hw, color, x1 - nxl * (hw + aa), y1 - nyl * (hw + aa), clear,
				x2 - nxl * (hw + aa), y2 - nyl * (hw + aa), clear, x2 - nxl * hw, y2 - nyl * hw, color);
	}

	/** Check mark used by checkboxes; p in 0..1 animates the stroke. */
	public static void check(float x, float y, float size, float width, float p, int color) {
		float ax = x + size * 0.18f, ay = y + size * 0.52f;
		float bx = x + size * 0.42f, by = y + size * 0.76f;
		float cx = x + size * 0.84f, cy = y + size * 0.26f;
		float p1 = Math.min(1f, p * 2f);
		line(ax, ay, ax + (bx - ax) * p1, ay + (by - ay) * p1, width, color);
		if (p > 0.5f) {
			float p2 = (p - 0.5f) * 2f;
			line(bx, by, bx + (cx - bx) * p2, by + (cy - by) * p2, width, color);
		}
	}

	// ---- text -----------------------------------------------------------------------------------

	public static int width(String s) {
		return Mc.font().width(s);
	}

	public static float width(String s, float scale) {
		return Mc.font().width(s) * scale;
	}

	public static int fontHeight() {
		return Mc.font().lineHeight;
	}

	public static void text(String s, float x, float y, int color) {
		text(s, x, y, color, false, 1f);
	}

	public static void textShadow(String s, float x, float y, int color) {
		text(s, x, y, color, true, 1f);
	}

	public static void text(String s, float x, float y, int color, boolean shadow, float scale) {
		if (s == null || s.isEmpty() || context == null) return;
		color = a(color);
		if (ColorUtil.alpha(color) < 8) return; // the font treats tiny alpha as opaque
		flush();
		Backend2D.text(context, s, X(x), Y(y), sx * scale, sy * scale, color, shadow);
	}

	public static void textCentered(String s, float cx, float y, int color, boolean shadow, float scale) {
		text(s, cx - width(s) * scale / 2f, y, color, shadow, scale);
	}

	/** Text with a per-character horizontal gradient (watermarks, titles). */
	public static void textGradient(String s, float x, float y, int c1, int c2, boolean shadow, float scale, float phase) {
		if (s == null || s.isEmpty()) return;
		float total = width(s) * scale;
		float cx = x;
		int n = s.length();
		for (int i = 0; i < n; i++) {
			String ch = s.substring(i, i + 1);
			float t = total <= 0 ? 0f : (cx - x) / total;
			float k = (float) (Math.sin((t + phase) * Math.PI * 2) * 0.5 + 0.5);
			text(ch, cx, y, ColorUtil.lerp(c1, c2, k), shadow, scale);
			cx += width(ch) * scale;
		}
	}

	/** Cuts text with "..." so it fits into maxWidth. */
	public static String ellipsize(String s, float maxWidth, float scale) {
		if (width(s) * scale <= maxWidth) return s;
		String dots = "...";
		float dw = width(dots) * scale;
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < s.length(); i++) {
			sb.append(s.charAt(i));
			if (width(sb.toString()) * scale + dw > maxWidth) {
				sb.setLength(Math.max(0, sb.length() - 1));
				break;
			}
		}
		return sb + dots;
	}

	// ---- items ----------------------------------------------------------------------------------

	/** Draws an item icon; size in GUI units (16 = normal). */
	public static void item(ItemStack stack, float x, float y, float size, boolean decorations) {
		if (stack == null || stack.isEmpty() || context == null) return;
		flush();
		Backend2D.item(context, stack, X(x), Y(y), size / 16f * sx, decorations);
	}
}
