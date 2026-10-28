package dev.rteam.visuals.render;

import java.util.Arrays;

/** Colored 2D or 3D quads waiting to be drawn: per vertex x, y, z and an ARGB color. */
public final class QuadBuffer {
	private float[] pos = new float[4 * 3 * 256];
	private int[] col = new int[4 * 256];
	private int quads;

	// bounds of the 2D content (for GUI render states)
	public float minX, minY, maxX, maxY;

	public void clear() {
		quads = 0;
		minX = minY = Float.MAX_VALUE;
		maxX = maxY = -Float.MAX_VALUE;
	}

	public QuadBuffer() {
		clear();
	}

	public boolean isEmpty() {
		return quads == 0;
	}

	public int size() {
		return quads;
	}

	private void ensure() {
		if ((quads + 1) * 4 > col.length) {
			col = Arrays.copyOf(col, col.length * 2);
			pos = Arrays.copyOf(pos, pos.length * 2);
		}
	}

	public void add(float x0, float y0, float z0, int c0,
					float x1, float y1, float z1, int c1,
					float x2, float y2, float z2, int c2,
					float x3, float y3, float z3, int c3) {
		if ((c0 | c1 | c2 | c3) >>> 24 == 0) return; // fully transparent
		ensure();
		int v = quads * 4;
		int p = v * 3;
		pos[p] = x0; pos[p + 1] = y0; pos[p + 2] = z0;
		pos[p + 3] = x1; pos[p + 4] = y1; pos[p + 5] = z1;
		pos[p + 6] = x2; pos[p + 7] = y2; pos[p + 8] = z2;
		pos[p + 9] = x3; pos[p + 10] = y3; pos[p + 11] = z3;
		col[v] = c0;
		col[v + 1] = c1;
		col[v + 2] = c2;
		col[v + 3] = c3;
		quads++;
		minX = Math.min(minX, Math.min(Math.min(x0, x1), Math.min(x2, x3)));
		maxX = Math.max(maxX, Math.max(Math.max(x0, x1), Math.max(x2, x3)));
		minY = Math.min(minY, Math.min(Math.min(y0, y1), Math.min(y2, y3)));
		maxY = Math.max(maxY, Math.max(Math.max(y0, y1), Math.max(y2, y3)));
	}

	public float x(int vertex) {
		return pos[vertex * 3];
	}

	public float y(int vertex) {
		return pos[vertex * 3 + 1];
	}

	public float z(int vertex) {
		return pos[vertex * 3 + 2];
	}

	public int color(int vertex) {
		return col[vertex];
	}

	/** Number of vertices (4 per quad). */
	public int vertices() {
		return quads * 4;
	}

	/** Copies the content (used when drawing happens later than recording). */
	public QuadBuffer copy() {
		QuadBuffer b = new QuadBuffer();
		b.pos = Arrays.copyOf(pos, Math.max(12, quads * 12));
		b.col = Arrays.copyOf(col, Math.max(4, quads * 4));
		b.quads = quads;
		b.minX = minX;
		b.minY = minY;
		b.maxX = maxX;
		b.maxY = maxY;
		return b;
	}
}
