package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.item.ItemStack;

import java.util.ArrayList;
import java.util.List;

/** Worn armor and held items with durability. */
public final class ArmorHud extends HudModule {
	private final ModeSetting layout = add(new ModeSetting("layout", Lang.of("Layout", "Расположение"), 0,
			new String[]{"horizontal", "vertical"}, new Lang[]{Lang.of("Horizontal", "Горизонтально"), Lang.of("Vertical", "Вертикально")}));
	private final BoolSetting hands = add(new BoolSetting("hands", Lang.of("Held items", "Предметы в руках"), true));
	private final BoolSetting percent = add(new BoolSetting("percent", Lang.of("Durability %", "Прочность в %"), true));

	public ArmorHud() {
		super("armorhud", Lang.of("Armor HUD", "Броня"), Lang.of("Armor and durability", "Броня и её прочность"), 0.5f, 0.82f);
		setEnabled(true, false);
	}

	private List<ItemStack> items(boolean editor) {
		List<ItemStack> out = new ArrayList<>();
		if (Mc.player() == null) return out;
		EquipmentSlot[] slots = hands.on()
				? new EquipmentSlot[]{EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET, EquipmentSlot.MAINHAND, EquipmentSlot.OFFHAND}
				: new EquipmentSlot[]{EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET};
		for (EquipmentSlot s : slots) {
			ItemStack st = Mc.player().getItemBySlot(s);
			if (!st.isEmpty()) out.add(st);
		}
		return out;
	}

	@Override
	protected boolean visible() {
		return !items(false).isEmpty();
	}

	@Override
	protected void draw(float pt, boolean editor) {
		List<ItemStack> list = items(editor);
		boolean vertical = layout.is("vertical");
		int n = Math.max(list.size(), editor ? 4 : 0);
		if (n == 0) {
			width = 20;
			height = 20;
			return;
		}
		float cell = 20;
		float w = vertical ? 46 : n * cell + 4;
		float h = vertical ? n * cell + 4 : 34;
		HudStyle.panel(0, 0, w, h);
		for (int i = 0; i < n; i++) {
			float x = vertical ? 4 : 4 + i * cell;
			float y = vertical ? 4 + i * cell : 4;
			if (i >= list.size()) {
				R2D.roundRect(x + 1, y + 1, 14, 14, 3, Theme.stroke());
				continue;
			}
			ItemStack st = list.get(i);
			R2D.item(st, x, y, 16, false);
			if (st.isDamageableItem() && st.getMaxDamage() > 0) {
				float f = 1f - st.getDamageValue() / (float) st.getMaxDamage();
				int c = ColorUtil.lerp(0xFFFF5050, 0xFF50FF80, f);
				String s = percent.on() ? Math.round(f * 100) + "%" : Integer.toString(st.getMaxDamage() - st.getDamageValue());
				if (vertical) R2D.text(s, x + 19, y + 4, c);
				else R2D.text(s, x + 8 - R2D.width(s) * 0.7f / 2f, y + 18, c, false, 0.7f);
			} else if (st.getCount() > 1) {
				String s = Integer.toString(st.getCount());
				if (vertical) R2D.text(s, x + 19, y + 4, Theme.text());
				else R2D.text(s, x + 8 - R2D.width(s) * 0.7f / 2f, y + 18, Theme.text(), false, 0.7f);
			}
		}
		width = w;
		height = h;
	}
}
