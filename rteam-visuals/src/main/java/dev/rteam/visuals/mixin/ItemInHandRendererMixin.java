package dev.rteam.visuals.mixin;

//#if MC < 26.3
import com.mojang.blaze3d.vertex.PoseStack;
import dev.rteam.visuals.modules.player.ViewModel;
import net.minecraft.client.renderer.ItemInHandRenderer;
import net.minecraft.world.entity.HumanoidArm;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Hand / held item position, size and rotation. */
@Mixin(ItemInHandRenderer.class)
public class ItemInHandRendererMixin {
	@Inject(method = "applyItemArmTransform", at = @At("TAIL"))
	private void rv$viewModel(PoseStack pose, HumanoidArm arm, float equipProgress, CallbackInfo ci) {
		ViewModel.apply(pose, arm == HumanoidArm.RIGHT);
	}
}
//#endif
