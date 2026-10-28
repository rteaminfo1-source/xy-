package dev.rteam.visuals.mixin;

//#if MC < 26.1
import dev.rteam.visuals.modules.world.Ambience;
import net.minecraft.client.multiplayer.ClientLevel;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/** Client-side time of day ("Ambience"). */
@Mixin(ClientLevel.ClientLevelData.class)
public class ClientLevelDataMixin {
	@Inject(method = "getDayTime", at = @At("RETURN"), cancellable = true)
	private void rv$dayTime(CallbackInfoReturnable<Long> cir) {
		long changed = Ambience.dayTime(cir.getReturnValue());
		if (changed != cir.getReturnValue()) cir.setReturnValue(changed);
	}
}
//#endif
