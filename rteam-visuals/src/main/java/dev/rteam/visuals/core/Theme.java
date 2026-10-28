package dev.rteam.visuals.core;

import dev.rteam.visuals.util.ColorUtil;

/** Colors of the menu and HUD. Updated from the "Theme" module settings. */
public final class Theme {
	private Theme() {
	}

	public static final String[] PRESETS = {"Aurora", "Sunset", "Ocean", "Mint", "Sakura", "Lava", "Royal", "Mono"};
	private static final int[][] PRESET_COLORS = {
			{0xFF7F5AF0, 0xFF2CB1FF}, // Aurora
			{0xFFFF7A59, 0xFFFF3D7F}, // Sunset
			{0xFF00B4DB, 0xFF0083B0}, // Ocean
			{0xFF2AF598, 0xFF009EFD}, // Mint
			{0xFFFF9A9E, 0xFFFAD0C4}, // Sakura
			{0xFFFF512F, 0xFFF09819}, // Lava
			{0xFF8E2DE2, 0xFF4A00E0}, // Royal
			{0xFFE0E0E0, 0xFF8A8A8A}, // Mono
	};

	public static int accent1 = PRESET_COLORS[0][0];
	public static int accent2 = PRESET_COLORS[0][1];
	public static boolean dark = true;
	public static float gradientSpeed = 1f;

	public static void applyPreset(String name) {
		for (int i = 0; i < PRESETS.length; i++) {
			if (PRESETS[i].equals(name)) {
				accent1 = PRESET_COLORS[i][0];
				accent2 = PRESET_COLORS[i][1];
				return;
			}
		}
	}

	public static int presetColor(String name, int index) {
		for (int i = 0; i < PRESETS.length; i++) {
			if (PRESETS[i].equals(name)) return PRESET_COLORS[i][index];
		}
		return PRESET_COLORS[0][index];
	}

	/** Animated two-color gradient; offset in 0..1 shifts along it. */
	public static int gradient(float offset) {
		return ColorUtil.wave(accent1, accent2, gradientSpeed, offset);
	}

	public static int accent() {
		return accent1;
	}

	// ---- surfaces
	public static int bg() {
		return dark ? 0xF0121218 : 0xF0F4F5FA;
	}

	public static int panel() {
		return dark ? 0xF01A1A23 : 0xF0FFFFFF;
	}

	public static int panel2() {
		return dark ? 0xF022222D : 0xF0ECEDF3;
	}

	public static int panel3() {
		return dark ? 0xF02C2C3A : 0xF0E0E2EA;
	}

	public static int stroke() {
		return dark ? 0x30FFFFFF : 0x30000000;
	}

	public static int text() {
		return dark ? 0xFFF2F2F7 : 0xFF1C1C24;
	}

	public static int textDim() {
		return dark ? 0xFF9A9AB0 : 0xFF6B6B7B;
	}

	public static int textFaint() {
		return dark ? 0xFF5E5E72 : 0xFFA0A0B0;
	}

	public static int hudBg() {
		return dark ? 0xB0101016 : 0xB0FFFFFF;
	}

	public static int shadow() {
		return 0x70000000;
	}
}
