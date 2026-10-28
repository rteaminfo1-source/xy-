package dev.rteam.visuals.mixin;

import dev.rteam.visuals.modules.world.Weather;
import net.minecraft.world.level.Level;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/** Client-side weather. */
@Mixin(Level.class)
public abstract class LevelMixin {
	@Inject(method = "getRainLevel", at = @At("RETURN"), cancellable = true)
	private void rv$rain(float partialTick, CallbackInfoReturnable<Float> cir) {
		if (!((Level) (Object) this).isClientSide()) return;
		float changed = Weather.rain(cir.getReturnValue());
		if (changed != cir.getReturnValue()) cir.setReturnValue(changed);
	}

	@Inject(method = "getThunderLevel", at = @At("RETURN"), cancellable = true)
	private void rv$thunder(float partialTick, CallbackInfoReturnable<Float> cir) {
		if (!((Level) (Object) this).isClientSide()) return;
		float changed = Weather.thunder(cir.getReturnValue());
		if (changed != cir.getReturnValue()) cir.setReturnValue(changed);
	}
}
