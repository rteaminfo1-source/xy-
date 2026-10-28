package dev.rteam.visuals.render;

import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.util.ColorUtil;

/**
 * World-space drawing. Modules call these during {@code onRender3D}; quads are stored relative to the
 * camera and drawn by {@link Backend3D} at the version specific hook (translucent, depth tested, no culling).
 */
public final class R3D {
	private R3D() {
	}

	private static final QuadBuffer QUADS = new QuadBuffer();
	private static double camX, camY, camZ;
	private static float partialTick;
	private static boolean recording;

	/** Collects this frame's world geometry from all modules; returns null if nothing was drawn. */
	public static QuadBuffer collect(double cx, double cy, double cz, float pt) {
		camX = cx;
		camY = cy;
		camZ = cz;
		partialTick = pt;
		QUADS.clear();
		recording = true;
		try {
			ModuleManager.render3D(pt);
		} finally {
			recording = false;
		}
		return QUADS.isEmpty() ? null : QUADS;
	}

	public static boolean recording() {
		return recording;
	}

	public static float partialTick() {
		return partialTick;
	}

	public static double camX() {
		return camX;
	}

	public static double camY() {
		return camY;
	}

	public static double camZ() {
		return camZ;
	}

	// ---- primitives (world coordinates) ---------------------------------------------------------

	public static void quad(double x0, double y0, double z0, int c0, double x1, double y1, double z1, int c1,
							double x2, double y2, double z2, int c2, double x3, double y3, double z3, int c3) {
		QUADS.add((float) (x0 - camX), (float) (y0 - camY), (float) (z0 - camZ), c0,
				(float) (x1 - camX), (float) (y1 - camY), (float) (z1 - camZ), c1,
				(float) (x2 - camX), (float) (y2 - camY), (float) (z2 - camZ), c2,
				(float) (x3 - camX), (float) (y3 - camY), (float) (z3 - camZ), c3);
	}

	public static void triangle(double x0, double y0, double z0, int c0, double x1, double y1, double z1, int c1,
								double x2, double y2, double z2, int c2) {
		quad(x0, y0, z0, c0, x1, y1, z1, c1, x2, y2, z2, c2, x2, y2, z2, c2);
	}

	/** Ribbon between two points that always faces the camera (looks like a thick line). */
	public static void line(double x1, double y1, double z1, double x2, double y2, double z2, float width, int c1, int c2) {
		double dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
		double mx = (x1 + x2) / 2 - camX, my = (y1 + y2) / 2 - camY, mz = (z1 + z2) / 2 - camZ;
		// n = dir x toCamera
		double nx = dy * (-mz) - dz * (-my);
		double ny = dz * (-mx) - dx * (-mz);
		double nz = dx * (-my) - dy * (-mx);
		double len = Math.sqrt(nx * nx + ny * ny + nz * nz);
		if (len < 1e-6) return;
		double k = width / 2.0 / len;
		nx *= k;
		ny *= k;
		nz *= k;
		quad(x1 + nx, y1 + ny, z1 + nz, c1, x1 - nx, y1 - ny, z1 - nz, c1, x2 - nx, y2 - ny, z2 - nz, c2, x2 + nx, y2 + ny, z2 + nz, c2);
	}

	/** Flat ring lying on the ground (normal up). Colors may differ inside/outside for soft edges. */
	public static void ring(double cx, double y, double cz, double radius, double thickness, int inner, int outer, int segments, double rotationDeg) {
		double r0 = Math.max(0, radius - thickness), r1 = radius;
		for (int i = 0; i < segments; i++) {
			double a0 = Math.toRadians(rotationDeg + 360.0 * i / segments);
			double a1 = Math.toRadians(rotationDeg + 360.0 * (i + 1) / segments);
			double c0 = Math.cos(a0), s0 = Math.sin(a0), c1 = Math.cos(a1), s1 = Math.sin(a1);
			quad(cx + c0 * r0, y, cz + s0 * r0, inner,
					cx + c0 * r1, y, cz + s0 * r1, outer,
					cx + c1 * r1, y, cz + s1 * r1, outer,
					cx + c1 * r0, y, cz + s1 * r0, inner);
		}
	}

	/** Ring whose color follows a function of the angle (gradients, rainbows). */
	public interface AngleColor {
		int color(double fraction);
	}

	public static void ringGradient(double cx, double y, double cz, double radius, double thickness, AngleColor color,
									float innerAlpha, float outerAlpha, int segments, double rotationDeg) {
		double r0 = Math.max(0, radius - thickness), r1 = radius;
		for (int i = 0; i < segments; i++) {
			double f0 = i / (double) segments, f1 = (i + 1) / (double) segments;
			double a0 = Math.toRadians(rotationDeg + 360.0 * f0);
			double a1 = Math.toRadians(rotationDeg + 360.0 * f1);
			int k0 = color.color(f0), k1 = color.color(f1);
			quad(cx + Math.cos(a0) * r0, y, cz + Math.sin(a0) * r0, ColorUtil.fade(k0, innerAlpha),
					cx + Math.cos(a0) * r1, y, cz + Math.sin(a0) * r1, ColorUtil.fade(k0, outerAlpha),
					cx + Math.cos(a1) * r1, y, cz + Math.sin(a1) * r1, ColorUtil.fade(k1, outerAlpha),
					cx + Math.cos(a1) * r0, y, cz + Math.sin(a1) * r0, ColorUtil.fade(k1, innerAlpha));
		}
	}

	/** Vertical band around a center (cylinder wall), fading towards the top. */
	public static void wall(double cx, double y, double cz, double radius, double height, AngleColor color,
							float bottomAlpha, float topAlpha, int segments, double rotationDeg) {
		for (int i = 0; i < segments; i++) {
			double f0 = i / (double) segments, f1 = (i + 1) / (double) segments;
			double a0 = Math.toRadians(rotationDeg + 360.0 * f0);
			double a1 = Math.toRadians(rotationDeg + 360.0 * f1);
			int k0 = color.color(f0), k1 = color.color(f1);
			double x0 = cx + Math.cos(a0) * radius, z0 = cz + Math.sin(a0) * radius;
			double x1 = cx + Math.cos(a1) * radius, z1 = cz + Math.sin(a1) * radius;
			quad(x0, y, z0, ColorUtil.fade(k0, bottomAlpha), x0, y + height, z0, ColorUtil.fade(k0, topAlpha),
					x1, y + height, z1, ColorUtil.fade(k1, topAlpha), x1, y, z1, ColorUtil.fade(k1, bottomAlpha));
		}
	}

	/** Cone with the tip above the base center (China hat). */
	public static void cone(double cx, double y, double cz, double radius, double height, AngleColor color,
							float rimAlpha, float tipAlpha, int segments, double rotationDeg) {
		for (int i = 0; i < segments; i++) {
			double f0 = i / (double) segments, f1 = (i + 1) / (double) segments;
			double a0 = Math.toRadians(rotationDeg + 360.0 * f0);
			double a1 = Math.toRadians(rotationDeg + 360.0 * f1);
			int k0 = color.color(f0), k1 = color.color(f1);
			int tip = ColorUtil.fade(ColorUtil.lerp(k0, k1, 0.5f), tipAlpha);
			triangle(cx + Math.cos(a0) * radius, y, cz + Math.sin(a0) * radius, ColorUtil.fade(k0, rimAlpha),
					cx + Math.cos(a1) * radius, y, cz + Math.sin(a1) * radius, ColorUtil.fade(k1, rimAlpha),
					cx, y + height, cz, tip);
		}
	}
}
