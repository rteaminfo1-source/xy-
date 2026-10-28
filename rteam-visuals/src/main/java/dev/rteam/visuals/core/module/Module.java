package dev.rteam.visuals.core.module;

import dev.rteam.visuals.core.setting.KeySetting;
import dev.rteam.visuals.core.setting.Setting;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.Lang;
import net.minecraft.world.entity.Entity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public abstract class Module {
	private final String id;
	private final Lang name;
	private final Lang description;
	private final Category category;
	private boolean enabled;
	private final List<Setting<?>> settings = new ArrayList<>();
	public final KeySetting key;

	/** 0..1 slide animation used by the module list HUD. */
	public final Anim listAnim = new Anim(0f, 260, Anim.Easing.OUT_CUBIC);

	protected Module(String id, Lang name, Lang description, Category category) {
		this.id = id;
		this.name = name;
		this.description = description;
		this.category = category;
		this.key = new KeySetting("key", Lang.of("Keybind", "Клавиша"), KeySetting.NONE);
	}

	protected <S extends Setting<?>> S add(S setting) {
		settings.add(setting);
		return setting;
	}

	public String id() {
		return id;
	}

	public String name() {
		return name.get();
	}

	/** Name used in the module list HUD (always English, like most visuals clients). */
	public String listName() {
		return name.en();
	}

	public String description() {
		return description.get();
	}

	public Category category() {
		return category;
	}

	public List<Setting<?>> settings() {
		return Collections.unmodifiableList(settings);
	}

	public boolean isEnabled() {
		return enabled;
	}

	public void toggle() {
		setEnabled(!enabled);
	}

	public void setEnabled(boolean value) {
		setEnabled(value, true);
	}

	public void setEnabled(boolean value, boolean notify) {
		if (enabled == value) return;
		enabled = value;
		listAnim.to(value ? 1f : 0f);
		try {
			if (value) onEnable();
			else onDisable();
		} catch (Throwable t) {
			ModuleManager.report(this, t);
		}
		if (notify) ModuleManager.onToggled(this);
	}

	/** Extra text shown next to the name in the module list (mode etc.). */
	public String suffix() {
		return null;
	}

	/** Whether the module appears in the module list HUD. */
	public boolean showInList() {
		return true;
	}

	// ---- events -------------------------------------------------------------------------------

	protected void onEnable() {
	}

	protected void onDisable() {
	}

	public void onTick() {
	}

	/** 2D drawing on the HUD (R2D is ready). */
	public void onRender2D(float partialTick) {
	}

	/** World drawing (R3D is ready). */
	public void onRender3D(float partialTick) {
	}

	public void onAttack(Entity target) {
	}

	public void onJump() {
	}

	public void onWorldJoin() {
	}
}
