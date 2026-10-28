package dev.rteam.visuals.mixin;

import dev.rteam.visuals.Hooks;
import dev.rteam.visuals.modules.player.SwingAnimation;
import net.minecraft.world.entity.LivingEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(LivingEntity.class)
public abstract class LivingEntityMixin {
	@Inject(method = "jumpFromGround", at = @At("HEAD"))
	private void rv$jump(CallbackInfo ci) {
		Hooks.jump((LivingEntity) (Object) this);
	}

	//#if MC < 26.3
	@Inject(method = "getCurrentSwingDuration", at = @At("RETURN"), cancellable = true)
	private void rv$swingDuration(CallbackInfoReturnable<Integer> cir) {
		int changed = SwingAnimation.duration((LivingEntity) (Object) this, cir.getReturnValue());
		if (changed != cir.getReturnValue()) cir.setReturnValue(changed);
	}
	//#endif
}
