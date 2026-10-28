package dev.rteam.visuals.core.module;

import dev.rteam.visuals.util.Lang;

public enum Category {
	HUD(Lang.of("HUD", "Интерфейс"), "H"),
	RENDER(Lang.of("Render", "Отрисовка"), "R"),
	WORLD(Lang.of("World", "Мир"), "W"),
	PLAYER(Lang.of("Player", "Игрок"), "P"),
	MISC(Lang.of("Client", "Клиент"), "C");

	private final Lang name;
	private final String icon;

	Category(Lang name, String icon) {
		this.name = name;
		this.icon = icon;
	}

	public String displayName() {
		return name.get();
	}

	/** Icon id used by the menu's vector icon painter. */
	public String icon() {
		return icon;
	}
}
