package dev.rteam.visuals.mixin;

import dev.rteam.visuals.Hooks;
import dev.rteam.visuals.modules.render.Crosshair;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
//#if MC >= 26.2
import net.minecraft.client.gui.Hud;
//#else
import net.minecraft.client.gui.Gui;
//#endif
//#if MC >= 26.1
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.GuiGraphicsExtractor;
//#elseif MC >= 1.21
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.GuiGraphics;
//#elseif MC >= 1.20
import net.minecraft.client.gui.GuiGraphics;
//#else
import com.mojang.blaze3d.vertex.PoseStack;
//#endif

/** Draws our HUD after the vanilla one and hides the vanilla crosshair when ours is on. */
//#if MC >= 26.2
@Mixin(Hud.class)
//#else
@Mixin(Gui.class)
//#endif
public class HudMixin {
	//#if MC >= 26.1
	@Inject(method = "extractRenderState", at = @At("TAIL"))
	private void rv$hud(GuiGraphicsExtractor graphics, DeltaTracker delta, CallbackInfo ci) {
		Hooks.hud(graphics, delta.getGameTimeDeltaPartialTick(false));
	}

	@Inject(method = "extractCrosshair", at = @At("HEAD"), cancellable = true)
	private void rv$crosshair(CallbackInfo ci) {
		if (Crosshair.hideVanilla()) ci.cancel();
	}
	//#elseif MC >= 1.21
	@Inject(method = "render", at = @At("TAIL"))
	private void rv$hud(GuiGraphics graphics, DeltaTracker delta, CallbackInfo ci) {
		Hooks.hud(graphics, delta.getGameTimeDeltaPartialTick(false));
	}

	@Inject(method = "renderCrosshair", at = @At("HEAD"), cancellable = true)
	private void rv$crosshair(CallbackInfo ci) {
		if (Crosshair.hideVanilla()) ci.cancel();
	}
	//#elseif MC >= 1.20
	@Inject(method = "render", at = @At("TAIL"))
	private void rv$hud(GuiGraphics graphics, float partialTick, CallbackInfo ci) {
		Hooks.hud(graphics, partialTick);
	}

	@Inject(method = "renderCrosshair", at = @At("HEAD"), cancellable = true)
	private void rv$crosshair(CallbackInfo ci) {
		if (Crosshair.hideVanilla()) ci.cancel();
	}
	//#else
	@Inject(method = "render", at = @At("TAIL"))
	private void rv$hud(PoseStack pose, float partialTick, CallbackInfo ci) {
		Hooks.hud(pose, partialTick);
	}

	@Inject(method = "renderCrosshair", at = @At("HEAD"), cancellable = true)
	private void rv$crosshair(CallbackInfo ci) {
		if (Crosshair.hideVanilla()) ci.cancel();
	}
	//#endif
}
