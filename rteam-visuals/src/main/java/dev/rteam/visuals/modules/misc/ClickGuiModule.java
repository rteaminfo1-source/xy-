package dev.rteam.visuals.modules.misc;

import dev.rteam.visuals.api.Keys;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ColorSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.Lang;

/** Menu and theme settings; its key opens the menu. */
public final class ClickGuiModule extends Module {
	public final ModeSetting preset = add(new ModeSetting("preset", Lang.of("Theme", "Тема"), 0, presetIds(), presetNames()));
	public final ColorSetting color1 = add(new ColorSetting("accent1", Lang.of("Accent 1", "Акцент 1"), Theme.accent1));
	public final ColorSetting color2 = add(new ColorSetting("accent2", Lang.of("Accent 2", "Акцент 2"), Theme.accent2));
	public final BoolSetting dark = add(new BoolSetting("dark", Lang.of("Dark theme", "Тёмная тема"), true));
	public final BoolSetting blur = add(new BoolSetting("blur", Lang.of("Blur background", "Размытие фона"), true));
	public final BoolSetting particles = add(new BoolSetting("particles", Lang.of("Background particles", "Частицы на фоне"), true));
	public final NumberSetting animSpeed = add(new NumberSetting("anim", Lang.of("Animation speed", "Скорость анимаций"), 1.0, 0.5, 2.0, 0.1).suffix("x"));
	public final ModeSetting language = add(new ModeSetting("language", Lang.of("Language", "Язык"), 0,
			new String[]{"ru", "en"}, new Lang[]{Lang.of("Русский", "Русский"), Lang.of("English", "English")}));

	public ClickGuiModule() {
		super("clickgui", Lang.of("Menu & Theme", "Меню и тема"), Lang.of("Menu key, colors, language", "Клавиша меню, цвета, язык"), Category.MISC);
		color1.visibleWhen(() -> preset.is("custom"));
		color2.visibleWhen(() -> preset.is("custom"));
		key.set(Keys.RSHIFT);
		preset.onChange(this::apply);
		color1.onChange(this::apply);
		color2.onChange(this::apply);
		dark.onChange(this::apply);
		language.onChange(this::apply);
		animSpeed.onChange(this::apply);
		apply();
	}

	private static String[] presetIds() {
		String[] ids = new String[Theme.PRESETS.length + 1];
		System.arraycopy(Theme.PRESETS, 0, ids, 0, Theme.PRESETS.length);
		ids[ids.length - 1] = "custom";
		return ids;
	}

	private static Lang[] presetNames() {
		Lang[] names = new Lang[Theme.PRESETS.length + 1];
		for (int i = 0; i < Theme.PRESETS.length; i++) names[i] = Lang.of(Theme.PRESETS[i], Theme.PRESETS[i]);
		names[names.length - 1] = Lang.of("Custom", "Свои цвета");
		return names;
	}

	public void apply() {
		if (preset.is("custom")) {
			Theme.accent1 = color1.get();
			Theme.accent2 = color2.get();
		} else {
			Theme.applyPreset(preset.get());
		}
		Theme.dark = dark.on();
		Lang.russian = language.is("ru");
		Anim.speedFactor = (float) (1.0 / animSpeed.d());
	}

	@Override
	public boolean showInList() {
		return false;
	}

	@Override
	public void toggle() {
		// not a toggle: always "off", its key opens the menu
	}
}
