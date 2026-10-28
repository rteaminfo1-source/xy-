package dev.rteam.visuals.core.setting;

import com.google.gson.JsonElement;
import dev.rteam.visuals.util.Lang;

import java.util.function.BooleanSupplier;

public abstract class Setting<T> {
	private final String id;
	private final Lang name;
	private final T defaultValue;
	protected T value;
	private BooleanSupplier visible = null;
	private Runnable onChange = null;

	protected Setting(String id, Lang name, T defaultValue) {
		this.id = id;
		this.name = name;
		this.defaultValue = defaultValue;
		this.value = defaultValue;
	}

	public String id() {
		return id;
	}

	public String name() {
		return name.get();
	}

	public T get() {
		return value;
	}

	public void set(T v) {
		if (v == null) return;
		T old = this.value;
		this.value = v;
		if (onChange != null && !v.equals(old)) onChange.run();
	}

	public T defaultValue() {
		return defaultValue;
	}

	public void reset() {
		set(defaultValue);
	}

	@SuppressWarnings("unchecked")
	public <S extends Setting<T>> S visibleWhen(BooleanSupplier condition) {
		this.visible = condition;
		return (S) this;
	}

	@SuppressWarnings("unchecked")
	public <S extends Setting<T>> S onChange(Runnable r) {
		this.onChange = r;
		return (S) this;
	}

	public boolean isVisible() {
		return visible == null || visible.getAsBoolean();
	}

	public abstract JsonElement save();

	public abstract void load(JsonElement e);
}
