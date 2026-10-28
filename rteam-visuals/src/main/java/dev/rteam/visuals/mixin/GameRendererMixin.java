package dev.rteam.visuals.mixin;

import dev.rteam.visuals.modules.player.NoHurtCam;
import dev.rteam.visuals.modules.player.Zoom;
import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
//#if MC < 26.1
import net.minecraft.client.Camera;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;
//#endif

@Mixin(GameRenderer.class)
public class GameRendererMixin {
	@Inject(method = "bobHurt", at = @At("HEAD"), cancellable = true)
	private void rv$bobHurt(CallbackInfo ci) {
		if (NoHurtCam.active()) ci.cancel();
	}

	//#if MC >= 1.21.2 && MC < 26.1
	@Inject(method = "getFov", at = @At("RETURN"), cancellable = true)
	private void rv$fov(Camera camera, float partialTick, boolean useFovSetting, CallbackInfoReturnable<Float> cir) {
		if (useFovSetting) cir.setReturnValue((float) (cir.getReturnValue() * Zoom.fovMultiplier()));
	}
	//#elseif MC < 1.21.2
	@Inject(method = "getFov", at = @At("RETURN"), cancellable = true)
	private void rv$fov(Camera camera, float partialTick, boolean useFovSetting, CallbackInfoReturnable<Double> cir) {
		if (useFovSetting) cir.setReturnValue(cir.getReturnValue() * Zoom.fovMultiplier());
	}
	//#endif
}
