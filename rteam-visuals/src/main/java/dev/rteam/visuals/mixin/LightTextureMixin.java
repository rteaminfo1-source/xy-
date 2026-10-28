package dev.rteam.visuals.mixin;

//#if MC < 26.1
import dev.rteam.visuals.modules.world.Fullbright;
import net.minecraft.client.renderer.LightTexture;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Fullbright: the lightmap is computed with a raised gamma, the option itself is not changed. */
@Mixin(LightTexture.class)
public class LightTextureMixin {
	@Inject(method = "updateLightTexture", at = @At("HEAD"))
	private void rv$gammaPush(float partialTick, CallbackInfo ci) {
		Fullbright.push();
	}

	@Inject(method = "updateLightTexture", at = @At("RETURN"))
	private void rv$gammaPop(float partialTick, CallbackInfo ci) {
		Fullbright.pop();
	}
}
//#endif
