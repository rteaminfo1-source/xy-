package dev.rteam.visuals.mixin;

//#if MC >= 26.1
import dev.rteam.visuals.modules.world.Fullbright;
import net.minecraft.client.renderer.LightmapRenderStateExtractor;
import net.minecraft.client.renderer.state.LightmapRenderState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Fullbright for 26.x, where the lightmap is computed from a render state. */
@Mixin(LightmapRenderStateExtractor.class)
public class LightmapExtractorMixin {
	@Inject(method = "extract", at = @At("TAIL"))
	private void rv$fullbright(LightmapRenderState state, float partialTick, CallbackInfo ci) {
		if (!Fullbright.active()) return;
		state.brightness = Math.max(state.brightness, Fullbright.gamma());
		state.nightVisionEffectIntensity = Math.max(state.nightVisionEffectIntensity, 1f);
		state.needsUpdate = true;
	}
}
//#endif
