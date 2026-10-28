package dev.rteam.visuals.modules.render;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.world.entity.Entity;

import java.util.Random;

/** More particles when you hit something (client side only). */
public final class HitParticles extends Module {
	private final ModeSetting type = add(new ModeSetting("type", Lang.of("Particles", "Частицы"), 0,
			new String[]{"crit", "magic", "hearts", "totem", "flame", "soul", "endrod", "firework", "notes"},
			new Lang[]{Lang.of("Crits", "Криты"), Lang.of("Magic", "Магия"), Lang.of("Hearts", "Сердца"), Lang.of("Totem", "Тотем"),
					Lang.of("Flame", "Огонь"), Lang.of("Soul fire", "Огонь душ"), Lang.of("End rod", "Энд-стержень"),
					Lang.of("Firework", "Фейерверк"), Lang.of("Notes", "Ноты")}));
	private final NumberSetting amount = add(new NumberSetting("amount", Lang.of("Amount", "Количество"), 12, 1, 60, 1));
	private final NumberSetting spread = add(new NumberSetting("spread", Lang.of("Spread", "Разброс"), 0.6, 0.1, 2.0, 0.1));
	private final Random random = new Random();

	public HitParticles() {
		super("hitparticles", Lang.of("Hit Particles", "Частицы удара"), Lang.of("Extra particles when you hit", "Дополнительные частицы при ударе"), Category.RENDER);
	}

	@Override
	public String suffix() {
		return type.display();
	}

	private ParticleOptions particle() {
		switch (type.get()) {
			case "magic": return ParticleTypes.ENCHANTED_HIT;
			case "hearts": return ParticleTypes.HEART;
			case "totem": return ParticleTypes.TOTEM_OF_UNDYING;
			case "flame": return ParticleTypes.FLAME;
			case "soul": return ParticleTypes.SOUL_FIRE_FLAME;
			case "endrod": return ParticleTypes.END_ROD;
			case "firework": return ParticleTypes.FIREWORK;
			case "notes": return ParticleTypes.NOTE;
			default: return ParticleTypes.CRIT;
		}
	}

	@Override
	public void onAttack(Entity target) {
		if (Mc.level() == null) return;
		ParticleOptions p = particle();
		double s = spread.d();
		for (int i = 0; i < amount.i(); i++) {
			double x = target.getX() + (random.nextDouble() - 0.5) * target.getBbWidth();
			double y = target.getY() + random.nextDouble() * target.getBbHeight();
			double z = target.getZ() + (random.nextDouble() - 0.5) * target.getBbWidth();
			double vx = (random.nextDouble() - 0.5) * s;
			double vy = random.nextDouble() * s * 0.6;
			double vz = (random.nextDouble() - 0.5) * s;
			Mc.level().addParticle(p, x, y, z, vx, vy, vz);
		}
	}
}
