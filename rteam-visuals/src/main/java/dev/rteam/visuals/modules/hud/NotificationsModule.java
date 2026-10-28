package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;

/** Settings for the toasts in the corner. */
public final class NotificationsModule extends Module {
	public final BoolSetting onToggle = add(new BoolSetting("toggle", Lang.of("On module toggle", "При включении модулей"), true));
	public final NumberSetting duration = add(new NumberSetting("duration", Lang.of("Duration", "Длительность"), 2.0, 1.0, 6.0, 0.5).suffix(Lang.t("s", " с")));

	public NotificationsModule() {
		super("notifications", Lang.of("Notifications", "Уведомления"), Lang.of("Toasts in the corner of the screen", "Всплывающие уведомления в углу экрана"), Category.HUD);
		setEnabled(true, false);
	}

	@Override
	public boolean showInList() {
		return false;
	}
}
