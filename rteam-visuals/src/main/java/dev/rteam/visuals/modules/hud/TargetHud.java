package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;
import dev.rteam.visuals.util.TargetTracker;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;

/** Information about the entity you are fighting. */
public final class TargetHud extends HudModule {
	private final ModeSetting healthColor = add(new ModeSetting("health_color", Lang.of("Health bar", "Полоса здоровья"), 0,
			new String[]{"health", "theme"}, new Lang[]{Lang.of("By health", "По здоровью"), Lang.of("Theme", "Тема")}));
	private final BoolSetting showArmor = add(new BoolSetting("armor", Lang.of("Armor", "Броня"), true));
	private final BoolSetting showDistance = add(new BoolSetting("distance", Lang.of("Distance", "Дистанция"), true));

	private final Anim appear = new Anim(0f, 280, Anim.Easing.OUT_BACK);
	private float shownHealth = 20f;
	private float shownAbsorb;
	private LivingEntity last;

	public TargetHud() {
		super("targethud", Lang.of("Target HUD", "Цель (TargetHUD)"), Lang.of("Health and armor of your target", "Здоровье и броня противника"), 0.55f, 0.6f);
		setEnabled(true, false);
	}

	@Override
	protected boolean visible() {
		return TargetTracker.target() != null || appear.get() > 0.02f;
	}

	@Override
	protected void draw(float pt, boolean editor) {
		LivingEntity t = TargetTracker.target();
		if (t != null) last = t;
		appear.to(t != null || editor ? 1f : 0f);
		float w = 150, h = showArmor.on() ? 50 : 38;
		width = w;
		height = h;
		float a = editor ? 1f : appear.get();
		if (a <= 0.02f) return;
		LivingEntity e = editor && last == null ? Mc.player() : last;
		String name = e == null ? "Steve" : e.getName().getString();
		float hp = e == null ? 14f : e.getHealth();
		float max = e == null ? 20f : Math.max(1f, e.getMaxHealth());
		float absorb = e == null ? 4f : e.getAbsorptionAmount();
		float d = Math.min(1f, 10f * dev.rteam.visuals.Hooks.frameDelta());
		shownHealth = MathUtil.lerp(shownHealth, hp, d);
		shownAbsorb = MathUtil.lerp(shownAbsorb, absorb, d);

		R2D.push();
		R2D.scaleAround(0.85f + 0.15f * Math.min(1f, a), w / 2f, h / 2f);
		R2D.setAlpha(Math.min(1f, a));
		HudStyle.panel(0, 0, w, h);

		// avatar: gradient square with the first letter, red when hurt
		float hurt = e == null ? 0f : Math.min(1f, e.hurtTime / 10f);
		int c1 = ColorUtil.lerp(Theme.accent1, 0xFFFF4040, hurt);
		int c2 = ColorUtil.lerp(Theme.accent2, 0xFFFF8080, hurt);
		R2D.roundRect4(5, 5, 28, 28, 6, c1, c2, c1, c2);
		String letter = name.isEmpty() ? "?" : name.substring(0, 1).toUpperCase();
		R2D.text(letter, 19 - R2D.width(letter) * 1.6f / 2f, 19 - 6.4f, 0xFFFFFFFF, true, 1.6f);

		R2D.text(R2D.ellipsize(name, w - 46, 1f), 39, 6, Theme.text());
		String hpText = MathUtil.format(hp + absorb, 1) + " HP";
		if (showDistance.on() && e != null && Mc.player() != null && e != Mc.player()) {
			hpText += "  ·  " + MathUtil.format(e.distanceTo(Mc.player()), 1) + "m";
		}
		R2D.text(hpText, 39, 16, Theme.textDim());

		// health bar
		float bx = 39, by = 27, bw = w - 45, bh = 5;
		R2D.roundRect(bx, by, bw, bh, 2.5f, ColorUtil.withAlpha(0x000000, 90));
		float frac = MathUtil.clamp(shownHealth / max, 0f, 1f);
		int hc = healthColor.is("theme") ? Theme.gradient(0f) : ColorUtil.lerp(0xFFFF4D4D, 0xFF4ADE80, frac);
		int hc2 = healthColor.is("theme") ? Theme.gradient(0.5f) : ColorUtil.brighter(hc, 0.25f);
		if (frac > 0.01f) R2D.roundRectGradient(bx, by, bw * frac, bh, 2.5f, hc, hc2, true);
		if (shownAbsorb > 0.05f) {
			float af = MathUtil.clamp(shownAbsorb / max, 0f, 1f);
			R2D.roundRect(bx, by + bh + 1.5f, bw * af, 2f, 1f, 0xFFFFC94D);
		}

		if (showArmor.on() && e != null) {
			float ix = 5;
			EquipmentSlot[] slots = {EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET, EquipmentSlot.MAINHAND, EquipmentSlot.OFFHAND};
			for (EquipmentSlot s : slots) {
				ItemStack st = e.getItemBySlot(s);
				if (!st.isEmpty()) R2D.item(st, ix, 36, 11, false);
				ix += 13;
			}
		}
		R2D.setAlpha(1f);
		R2D.pop();
	}
}
