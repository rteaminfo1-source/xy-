package dev.rteam.visuals.util;

import dev.rteam.visuals.api.Mc;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;

/** Remembers the last living entity the player hit (for Target HUD and Target ESP). */
public final class TargetTracker {
	private TargetTracker() {
	}

	private static LivingEntity target;
	private static long lastHit;
	/** How long a target stays selected after the last hit. */
	public static long keepMs = 5000L;

	public static void onAttack(Entity e) {
		if (e instanceof LivingEntity) {
			target = (LivingEntity) e;
			lastHit = System.currentTimeMillis();
		}
	}

	public static void tick() {
		if (target == null) return;
		if (!target.isAlive() || Mc.level() == null
				|| System.currentTimeMillis() - lastHit > keepMs
				|| (Mc.player() != null && target.distanceTo(Mc.player()) > 32f)) {
			target = null;
		}
	}

	/** Current target or null. */
	public static LivingEntity target() {
		return target;
	}

	public static long sinceHit() {
		return System.currentTimeMillis() - lastHit;
	}
}
