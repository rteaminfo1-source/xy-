package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import net.minecraft.world.effect.MobEffectInstance;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Active potion effects with time left. */
public final class PotionHud extends HudModule {
	private final Map<String, Integer> maxDuration = new HashMap<>();

	public PotionHud() {
		super("potions", Lang.of("Potions", "Зелья"), Lang.of("Active effects with time left", "Активные эффекты и сколько осталось"), 0.01f, 0.35f);
		setEnabled(true, false);
	}

	@Override
	protected boolean visible() {
		return Mc.player() != null && !Mc.player().getActiveEffects().isEmpty();
	}

	@Override
	protected void draw(float pt, boolean editor) {
		List<String[]> rows = new ArrayList<>();
		List<Integer> colors = new ArrayList<>();
		List<Float> fracs = new ArrayList<>();
		if (Mc.player() != null) {
			List<MobEffectInstance> list = new ArrayList<>(Mc.player().getActiveEffects());
			Collections.sort(list, new Comparator<MobEffectInstance>() {
				@Override
				public int compare(MobEffectInstance a, MobEffectInstance b) {
					return Integer.compare(b.getDuration(), a.getDuration());
				}
			});
			for (MobEffectInstance e : list) {
				String name = Mc.effectName(e);
				int lvl = e.getAmplifier() + 1;
				if (lvl > 1) name += " " + HudStyle.roman(lvl);
				boolean inf = Mc.infinite(e);
				String key = name;
				Integer max = maxDuration.get(key);
				if (max == null || e.getDuration() > max) maxDuration.put(key, max = e.getDuration());
				rows.add(new String[]{name, inf ? "**:**" : HudStyle.time(e.getDuration())});
				colors.add(ColorUtil.withAlpha(Mc.effect(e).getColor(), 255));
				fracs.add(inf ? 1f : e.getDuration() / (float) Math.max(1, max));
			}
		}
		if (rows.isEmpty() && editor) {
			rows.add(new String[]{Lang.t("Speed II", "Скорость II"), "1:30"});
			colors.add(0xFF7CAFC6);
			fracs.add(0.6f);
			rows.add(new String[]{Lang.t("Strength", "Сила"), "0:45"});
			colors.add(0xFFFFC700);
			fracs.add(0.3f);
		}
		float w = 90;
		for (String[] r : rows) w = Math.max(w, R2D.width(r[0]) + R2D.width(r[1]) + 26);
		float rowH = 14;
		float h = 16 + rows.size() * rowH + 2;
		HudStyle.accentPanel(0, 0, w, h);
		HudStyle.header(Lang.t("Effects", "Эффекты"), 0, 0, w);
		float y = 16;
		for (int i = 0; i < rows.size(); i++) {
			int c = colors.get(i);
			R2D.circle(8, y + 5.5f, 2.5f, c);
			R2D.text(rows.get(i)[0], 15, y + 2, Theme.text());
			R2D.text(rows.get(i)[1], w - 5 - R2D.width(rows.get(i)[1]), y + 2, Theme.textDim());
			R2D.roundRect(15, y + 11, (w - 20) * fracs.get(i), 1.2f, 0.6f, ColorUtil.fade(c, 0.7f));
			y += rowH;
		}
		width = w;
		height = h;
	}
}
