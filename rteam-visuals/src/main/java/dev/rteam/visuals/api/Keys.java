package dev.rteam.visuals.api;

import com.mojang.blaze3d.platform.InputConstants;

import java.util.HashMap;
import java.util.Map;

/** Keyboard state polling and key names. Key codes are the ones Minecraft reports (InputConstants). */
public final class Keys {
	private Keys() {
	}

	//#if MC >= 1.17
	public static final int RSHIFT = InputConstants.KEY_RSHIFT;
	public static final int ESCAPE = InputConstants.KEY_ESCAPE;
	public static final int BACKSPACE = InputConstants.KEY_BACKSPACE;
	public static final int DELETE = InputConstants.KEY_DELETE;
	public static final int ENTER = InputConstants.KEY_RETURN;
	public static final int LEFT = InputConstants.KEY_LEFT;
	public static final int RIGHT = InputConstants.KEY_RIGHT;
	public static final int LCONTROL = InputConstants.KEY_LCONTROL;
	public static final int RCONTROL = InputConstants.KEY_RCONTROL;
	public static final int KEY_A = InputConstants.KEY_A;
	public static final int KEY_C = InputConstants.KEY_C;
	public static final int KEY_V = InputConstants.KEY_V;
	//#else
	public static final int RSHIFT = 344;
	public static final int ESCAPE = 256;
	public static final int BACKSPACE = 259;
	public static final int DELETE = 261;
	public static final int ENTER = 257;
	public static final int LEFT = 263;
	public static final int RIGHT = 262;
	public static final int LCONTROL = 341;
	public static final int RCONTROL = 345;
	public static final int KEY_A = 65;
	public static final int KEY_C = 67;
	public static final int KEY_V = 86;
	//#endif

	private static final Map<Integer, Boolean> LAST = new HashMap<>();

	public static boolean isDown(int key) {
		if (key < 0) return false;
		try {
			//#if MC >= 26.3
			return InputConstants.isKeyDown(key);
			//#elseif MC >= 1.21.9
			return InputConstants.isKeyDown(Mc.mc().getWindow(), key);
			//#else
			return InputConstants.isKeyDown(Mc.mc().getWindow().getWindow(), key);
			//#endif
		} catch (RuntimeException e) {
			return false;
		}
	}

	/** True once when the key goes down. */
	public static boolean pressed(int key) {
		if (key < 0) return false;
		boolean down = isDown(key);
		Boolean before = LAST.put(key, down);
		return down && (before == null || !before);
	}

	/** Forget edges while a screen is open, so closing the menu with its own key does not reopen it. */
	public static void resetEdges() {
		for (Map.Entry<Integer, Boolean> e : LAST.entrySet()) e.setValue(Boolean.TRUE);
	}

	public static boolean ctrl() {
		return isDown(LCONTROL) || isDown(RCONTROL);
	}

	public static String name(int key) {
		if (key < 0) return "-";
		try {
			String s = InputConstants.Type.KEYSYM.getOrCreate(key).getDisplayName().getString();
			if (s != null && !s.isEmpty()) return s.length() > 12 ? s.substring(0, 12) : s;
		} catch (RuntimeException ignored) {
		}
		return "#" + key;
	}
}
