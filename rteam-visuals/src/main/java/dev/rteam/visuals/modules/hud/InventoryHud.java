package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Lang;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.item.ItemStack;

/** The 27 main inventory slots. */
public final class InventoryHud extends HudModule {
	public InventoryHud() {
		super("inventory", Lang.of("Inventory", "Инвентарь"), Lang.of("Shows your inventory on screen", "Показывает инвентарь на экране"), 0.99f, 0.5f);
	}

	@Override
	protected void draw(float pt, boolean editor) {
		float cell = 18;
		float w = 9 * cell + 6, h = 3 * cell + 18;
		HudStyle.accentPanel(0, 0, w, h);
		HudStyle.header(Lang.t("Inventory", "Инвентарь"), 0, 0, w);
		Inventory inv = Mc.player() == null ? null : Mc.inventory(Mc.player());
		for (int i = 0; i < 27; i++) {
			float x = 3 + (i % 9) * cell;
			float y = 15 + (i / 9) * cell;
			R2D.roundRect(x + 1, y + 1, cell - 2, cell - 2, 3, Theme.stroke());
			if (inv != null) {
				ItemStack st = inv.getItem(9 + i);
				if (!st.isEmpty()) R2D.item(st, x + 1, y + 1, 16, true);
			}
		}
		width = w;
		height = h;
	}
}
