package dev.rteam.visuals;

import dev.rteam.visuals.api.Keys;
import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.config.Config;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.gui.Notifications;
import dev.rteam.visuals.gui.screen.ClickGui;
import dev.rteam.visuals.modules.misc.ClickGuiModule;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.TargetTracker;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;

/** Entry points called from the mixins. Keep the mixins thin: all logic lives here and in modules. */
public final class Hooks {
	private Hooks() {
	}

	private static boolean wasInGame;
	private static long lastFrameNanos;
	private static int frames;
	private static long fpsWindowStart;
	private static int fps;
	private static float frameDelta = 1f / 60f;

	// ---- tick -----------------------------------------------------------------------------------

	public static void clientTick() {
		try {
			boolean inGame = Mc.inGame();
			if (inGame && !wasInGame) ModuleManager.worldJoin();
			wasInGame = inGame;
			if (inGame) {
				ModuleManager.tick();
				TargetTracker.tick();
			}
			Config.tick();
		} catch (Throwable t) {
			RteamVisuals.LOG.error("[RTeam Visuals] tick failed", t);
		}
	}

	// ---- input (polled every frame, so short taps are not missed) ---------------------------------

	public static void pollKeys() {
		if (Mc.screen() != null) {
			Keys.resetEdges();
			return;
		}
		ClickGuiModule gui = ModuleManager.get(ClickGuiModule.class);
		if (gui != null && Keys.pressed(gui.key.get())) {
			Mc.setScreen(ClickGui.create());
			return;
		}
		for (dev.rteam.visuals.core.module.Module m : ModuleManager.all()) {
			if (m == gui) continue;
			if (m.key.bound() && Keys.pressed(m.key.get())) m.toggle();
		}
	}

	// ---- HUD ------------------------------------------------------------------------------------

	/** Called at the end of vanilla HUD rendering with the version's graphics object. */
	public static void hud(Object graphics, float partialTick) {
		long now = System.nanoTime();
		if (lastFrameNanos != 0L) frameDelta = Math.min(0.25f, (now - lastFrameNanos) / 1.0e9f);
		lastFrameNanos = now;
		frames++;
		if (now - fpsWindowStart >= 1_000_000_000L) {
			fps = frames;
			frames = 0;
			fpsWindowStart = now;
		}
		pollKeys();
		if (Mc.hideGui()) return;
		R2D.begin(graphics);
		try {
			ModuleManager.render2D(partialTick);
			Notifications.render();
		} catch (Throwable t) {
			RteamVisuals.LOG.error("[RTeam Visuals] HUD render failed", t);
		} finally {
			R2D.end();
		}
	}

	public static int fps() {
		return fps;
	}

	/** Seconds since the previous frame (for animations). */
	public static float frameDelta() {
		return frameDelta;
	}

	// ---- game events ----------------------------------------------------------------------------

	public static void attack(Entity target) {
		if (target == null) return;
		try {
			TargetTracker.onAttack(target);
			ModuleManager.attack(target);
		} catch (Throwable t) {
			RteamVisuals.LOG.error("[RTeam Visuals] attack hook failed", t);
		}
	}

	public static void jump(LivingEntity entity) {
		if (entity != Mc.player()) return;
		try {
			ModuleManager.jump();
		} catch (Throwable t) {
			RteamVisuals.LOG.error("[RTeam Visuals] jump hook failed", t);
		}
	}
}
