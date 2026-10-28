package dev.rteam.visuals.mixin;

import dev.rteam.visuals.modules.player.CameraTweaks;
import net.minecraft.client.Camera;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyVariable;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/** Third person camera distance and "no clip" through blocks. */
@Mixin(Camera.class)
public class CameraMixin {
	//#if MC >= 1.21
	@ModifyVariable(method = "getMaxZoom", at = @At("HEAD"), argsOnly = true)
	private float rv$distance(float distance) {
		return (float) CameraTweaks.distance(distance);
	}

	@Inject(method = "getMaxZoom", at = @At("HEAD"), cancellable = true)
	private void rv$noClip(float distance, CallbackInfoReturnable<Float> cir) {
		if (CameraTweaks.noClip()) cir.setReturnValue(distance);
	}
	//#else
	@ModifyVariable(method = "getMaxZoom", at = @At("HEAD"), argsOnly = true)
	private double rv$distance(double distance) {
		return CameraTweaks.distance(distance);
	}

	@Inject(method = "getMaxZoom", at = @At("HEAD"), cancellable = true)
	private void rv$noClip(double distance, CallbackInfoReturnable<Double> cir) {
		if (CameraTweaks.noClip()) cir.setReturnValue(distance);
	}
	//#endif
}
