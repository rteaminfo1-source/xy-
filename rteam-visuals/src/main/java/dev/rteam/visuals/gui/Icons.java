package dev.rteam.visuals.gui;

import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;

/** Small vector icons drawn with R2D (no textures, so they are sharp on every GUI scale). */
public final class Icons {
	private Icons() {
	}

	public static void draw(String id, float cx, float cy, float s, int c) {
		switch (id) {
			case "H":
				hud(cx, cy, s, c);
				break;
			case "R":
				eye(cx, cy, s, c);
				break;
			case "W":
				globe(cx, cy, s, c);
				break;
			case "P":
				person(cx, cy, s, c);
				break;
			case "C":
				gear(cx, cy, s, c);
				break;
			case "search":
				search(cx, cy, s, c);
				break;
			case "layout":
				layout(cx, cy, s, c);
				break;
			case "moon":
				moon(cx, cy, s, c);
				break;
			case "sun":
				sun(cx, cy, s, c);
				break;
			case "close":
				close(cx, cy, s, c);
				break;
			case "key":
				key(cx, cy, s, c);
				break;
			default:
				R2D.circle(cx, cy, s * 0.3f, c);
		}
	}

	private static void hud(float cx, float cy, float s, int c) {
		float h = s * 0.5f;
		R2D.roundOutline(cx - h, cy - h * 0.8f, s, s * 0.8f, s * 0.15f, s * 0.11f, c);
		R2D.roundRect(cx - h * 0.6f, cy - h * 0.35f, s * 0.28f, s * 0.12f, s * 0.06f, c);
		R2D.roundRect(cx - h * 0.6f, cy + h * 0.05f, s * 0.5f, s * 0.12f, s * 0.06f, c);
	}

	private static void eye(float cx, float cy, float s, int c) {
		float r = s * 0.62f;
		R2D.arc(cx, cy + s * 0.32f, r, s * 0.1f, 220, 320, c);
		R2D.arc(cx, cy - s * 0.32f, r, s * 0.1f, 40, 140, c);
		R2D.circle(cx, cy, s * 0.17f, c);
	}

	private static void globe(float cx, float cy, float s, int c) {
		float r = s * 0.48f;
		R2D.ring(cx, cy, r, s * 0.1f, c);
		R2D.rect(cx - r, cy - s * 0.05f, cx + r, cy + s * 0.05f, c);
		R2D.arc(cx + r * 0.9f, cy, r * 1.05f, s * 0.08f, 140, 220, c);
		R2D.arc(cx - r * 0.9f, cy, r * 1.05f, s * 0.08f, -40, 40, c);
	}

	private static void person(float cx, float cy, float s, int c) {
		R2D.circle(cx, cy - s * 0.2f, s * 0.2f, c);
		R2D.arc(cx, cy + s * 0.5f, s * 0.42f, s * 0.12f, 200, 340, c);
	}

	private static void gear(float cx, float cy, float s, int c) {
		R2D.ring(cx, cy, s * 0.32f, s * 0.12f, c);
		for (int i = 0; i < 8; i++) {
			double a = Math.PI * 2 * i / 8;
			R2D.circle(cx + (float) Math.cos(a) * s * 0.42f, cy + (float) Math.sin(a) * s * 0.42f, s * 0.085f, c);
		}
	}

	private static void search(float cx, float cy, float s, int c) {
		R2D.ring(cx - s * 0.1f, cy - s * 0.1f, s * 0.3f, s * 0.1f, c);
		R2D.line(cx + s * 0.12f, cy + s * 0.12f, cx + s * 0.4f, cy + s * 0.4f, s * 0.12f, c);
	}

	private static void layout(float cx, float cy, float s, int c) {
		float h = s * 0.45f;
		R2D.roundOutline(cx - h, cy - h, s * 0.9f, s * 0.9f, s * 0.12f, s * 0.1f, c);
		R2D.rect(cx - h, cy - h * 0.25f, cx + h, cy - h * 0.25f + s * 0.1f, c);
		R2D.rect(cx - s * 0.05f, cy - h * 0.25f, cx + s * 0.05f, cy + h, c);
	}

	private static void moon(float cx, float cy, float s, int c) {
		R2D.arc(cx, cy, s * 0.4f, s * 0.14f, 60, 330, c);
	}

	private static void sun(float cx, float cy, float s, int c) {
		R2D.circle(cx, cy, s * 0.2f, c);
		for (int i = 0; i < 8; i++) {
			double a = Math.PI * 2 * i / 8;
			float x1 = cx + (float) Math.cos(a) * s * 0.32f, y1 = cy + (float) Math.sin(a) * s * 0.32f;
			float x2 = cx + (float) Math.cos(a) * s * 0.45f, y2 = cy + (float) Math.sin(a) * s * 0.45f;
			R2D.line(x1, y1, x2, y2, s * 0.08f, c);
		}
	}

	private static void close(float cx, float cy, float s, int c) {
		float h = s * 0.3f;
		R2D.line(cx - h, cy - h, cx + h, cy + h, s * 0.12f, c);
		R2D.line(cx - h, cy + h, cx + h, cy - h, s * 0.12f, c);
	}

	private static void key(float cx, float cy, float s, int c) {
		R2D.ring(cx - s * 0.18f, cy, s * 0.2f, s * 0.09f, c);
		R2D.rect(cx, cy - s * 0.05f, cx + s * 0.45f, cy + s * 0.05f, c);
		R2D.rect(cx + s * 0.3f, cy, cx + s * 0.38f, cy + s * 0.2f, c);
	}

	/** iOS-like switch; p = 0..1 knob position. */
	public static void toggle(float x, float y, float w, float h, float p, int on, int off) {
		int bg = ColorUtil.lerp(off, on, p);
		R2D.roundRect(x, y, w, h, h / 2f, bg);
		float k = h - 3f;
		float kx = x + 1.5f + (w - k - 3f) * p;
		R2D.shadow(kx, y + 1.5f, k, k, k / 2f, 2f, ColorUtil.withAlpha(0, 60));
		R2D.circle(kx + k / 2f, y + h / 2f, k / 2f, 0xFFFFFFFF);
	}
}
