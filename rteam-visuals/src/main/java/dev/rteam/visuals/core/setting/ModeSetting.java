package dev.rteam.visuals.core.setting;

import com.google.gson.JsonElement;
import com.google.gson.JsonPrimitive;
import dev.rteam.visuals.util.Lang;

/** One choice out of a fixed list. Values are stored by their English id. */
public final class ModeSetting extends Setting<String> {
	private final String[] ids;
	private final Lang[] names;

	public ModeSetting(String id, Lang name, int defIndex, String[] ids, Lang[] names) {
		super(id, name, ids[defIndex]);
		this.ids = ids;
		this.names = names;
	}

	/** Modes whose display name is the same in both languages. */
	public ModeSetting(String id, Lang name, String def, String... modes) {
		super(id, name, def);
		this.ids = modes;
		this.names = new Lang[modes.length];
		for (int i = 0; i < modes.length; i++) names[i] = Lang.of(modes[i], modes[i]);
	}

	public boolean is(String mode) {
		return value.equals(mode);
	}

	public int index() {
		for (int i = 0; i < ids.length; i++) if (ids[i].equals(value)) return i;
		return 0;
	}

	public int count() {
		return ids.length;
	}

	public String idAt(int i) {
		return ids[i];
	}

	public String nameAt(int i) {
		return names[i].get();
	}

	public String display() {
		return names[index()].get();
	}

	public void cycle(int dir) {
		int i = (index() + dir + ids.length) % ids.length;
		set(ids[i]);
	}

	@Override
	public void set(String v) {
		for (String id : ids) {
			if (id.equals(v)) {
				super.set(v);
				return;
			}
		}
	}

	@Override
	public JsonElement save() {
		return new JsonPrimitive(value);
	}

	@Override
	public void load(JsonElement e) {
		if (e != null && e.isJsonPrimitive()) set(e.getAsString());
	}
}
