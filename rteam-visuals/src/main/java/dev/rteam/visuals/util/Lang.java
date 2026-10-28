package dev.rteam.visuals.util;

/** Two-language UI text (Russian is the default, English can be chosen in the menu). */
public final class Lang {
	public static boolean russian = true;

	private final String en;
	private final String ru;

	private Lang(String en, String ru) {
		this.en = en;
		this.ru = ru;
	}

	public static Lang of(String en, String ru) {
		return new Lang(en, ru);
	}

	public static String t(String en, String ru) {
		return russian ? ru : en;
	}

	public String get() {
		return russian ? ru : en;
	}

	public String en() {
		return en;
	}

	public String ru() {
		return ru;
	}

	@Override
	public String toString() {
		return get();
	}
}
