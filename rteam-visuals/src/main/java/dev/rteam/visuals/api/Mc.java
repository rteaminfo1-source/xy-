package dev.rteam.visuals.api;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.Font;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.multiplayer.ClientLevel;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.network.chat.Component;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;

/** Version independent access to the game. Everything that differs between versions goes through here. */
public final class Mc {
	private Mc() {
	}

	public static Minecraft mc() {
		return Minecraft.getInstance();
	}

	public static LocalPlayer player() {
		return mc().player;
	}

	public static ClientLevel level() {
		return mc().level;
	}

	public static boolean inGame() {
		return mc().player != null && mc().level != null;
	}

	public static Font font() {
		return mc().font;
	}

	public static Screen screen() {
		//#if MC >= 26.2
		return mc().gui.screen();
		//#else
		return mc().screen;
		//#endif
	}

	public static void setScreen(Screen screen) {
		//#if MC >= 26.2
		mc().gui.setScreen(screen);
		//#else
		mc().setScreen(screen);
		//#endif
	}

	public static boolean hideGui() {
		//#if MC >= 26.2
		return false;
		//#else
		return mc().options.hideGui;
		//#endif
	}

	public static int guiWidth() {
		return mc().getWindow().getGuiScaledWidth();
	}

	public static int guiHeight() {
		return mc().getWindow().getGuiScaledHeight();
	}

	public static double guiScale() {
		return mc().getWindow().getGuiScale();
	}

	/** Framebuffer size in pixels. */
	public static int pixelWidth() {
		return mc().getWindow().getWidth();
	}

	public static int pixelHeight() {
		return mc().getWindow().getHeight();
	}

	public static float partialTick() {
		//#if MC >= 1.21.3
		return mc().getDeltaTracker().getGameTimeDeltaPartialTick(true);
		//#elseif MC >= 1.21
		return mc().getTimer().getGameTimeDeltaPartialTick(true);
		//#else
		return mc().getFrameTime();
		//#endif
	}

	/** Mouse position in GUI coordinates. */
	public static double mouseX() {
		return mc().mouseHandler.xpos() * guiWidth() / Math.max(1, mc().getWindow().getScreenWidth());
	}

	public static double mouseY() {
		return mc().mouseHandler.ypos() * guiHeight() / Math.max(1, mc().getWindow().getScreenHeight());
	}

	public static Component literal(String s) {
		//#if MC >= 1.19
		return Component.literal(s);
		//#else
		return new net.minecraft.network.chat.TextComponent(s);
		//#endif
	}

	public static String serverAddress() {
		if (mc().getCurrentServer() != null) return mc().getCurrentServer().ip;
		return mc().isLocalServer() ? "singleplayer" : "";
	}

	public static int ping() {
		if (!inGame() || mc().getConnection() == null) return 0;
		net.minecraft.client.multiplayer.PlayerInfo info = mc().getConnection().getPlayerInfo(player().getUUID());
		return info == null ? 0 : info.getLatency();
	}

	// ---- entities -------------------------------------------------------------------------------

	public static double lerpX(Entity e, float pt) {
		return e.xo + (e.getX() - e.xo) * pt;
	}

	public static double lerpY(Entity e, float pt) {
		return e.yo + (e.getY() - e.yo) * pt;
	}

	public static double lerpZ(Entity e, float pt) {
		return e.zo + (e.getZ() - e.zo) * pt;
	}

	public static boolean onGround(Entity e) {
		//#if MC >= 1.20
		return e.onGround();
		//#else
		return e.isOnGround();
		//#endif
	}

	public static float yaw(Entity e) {
		//#if MC >= 1.17
		return e.getYRot();
		//#else
		return e.yRot;
		//#endif
	}

	public static Vec3 velocity(Entity e) {
		return e.getDeltaMovement();
	}

	// ---- effects --------------------------------------------------------------------------------

	public static MobEffect effect(MobEffectInstance inst) {
		//#if MC >= 1.20.5
		return inst.getEffect().value();
		//#else
		return inst.getEffect();
		//#endif
	}

	public static String effectName(MobEffectInstance inst) {
		return effect(inst).getDisplayName().getString();
	}

	public static boolean infinite(MobEffectInstance inst) {
		//#if MC >= 1.19.4
		return inst.isInfiniteDuration();
		//#else
		return false;
		//#endif
	}
}
