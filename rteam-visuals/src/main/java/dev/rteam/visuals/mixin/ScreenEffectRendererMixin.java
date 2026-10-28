package dev.rteam.visuals.mixin;

import com.mojang.blaze3d.vertex.PoseStack;
import dev.rteam.visuals.modules.player.LowFire;
import net.minecraft.client.renderer.ScreenEffectRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
//#if MC >= 26.2
import net.minecraft.client.renderer.SubmitNodeCollector;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
//#elseif MC >= 1.21.9
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.texture.TextureAtlasSprite;
//#elseif MC >= 1.21.4
import net.minecraft.client.renderer.MultiBufferSource;
//#else
import net.minecraft.client.Minecraft;
//#endif

/** Lowers the fire overlay ("LowFire"). */
@Mixin(ScreenEffectRenderer.class)
public class ScreenEffectRendererMixin {
	//#if MC >= 26.2
	@Inject(method = "submitFire", at = @At("HEAD"))
	private static void rv$fireStart(PoseStack pose, SubmitNodeCollector collector, TextureAtlasSprite sprite, CallbackInfo ci) {
		LowFire.start(pose);
	}

	@Inject(method = "submitFire", at = @At("RETURN"))
	private static void rv$fireEnd(PoseStack pose, SubmitNodeCollector collector, TextureAtlasSprite sprite, CallbackInfo ci) {
		LowFire.end(pose);
	}
	//#elseif MC >= 1.21.9
	@Inject(method = "renderFire", at = @At("HEAD"))
	private static void rv$fireStart(PoseStack pose, MultiBufferSource source, TextureAtlasSprite sprite, CallbackInfo ci) {
		LowFire.start(pose);
	}

	@Inject(method = "renderFire", at = @At("RETURN"))
	private static void rv$fireEnd(PoseStack pose, MultiBufferSource source, TextureAtlasSprite sprite, CallbackInfo ci) {
		LowFire.end(pose);
	}
	//#elseif MC >= 1.21.4
	@Inject(method = "renderFire", at = @At("HEAD"))
	private static void rv$fireStart(PoseStack pose, MultiBufferSource source, CallbackInfo ci) {
		LowFire.start(pose);
	}

	@Inject(method = "renderFire", at = @At("RETURN"))
	private static void rv$fireEnd(PoseStack pose, MultiBufferSource source, CallbackInfo ci) {
		LowFire.end(pose);
	}
	//#else
	@Inject(method = "renderFire", at = @At("HEAD"))
	private static void rv$fireStart(Minecraft mc, PoseStack pose, CallbackInfo ci) {
		LowFire.start(pose);
	}

	@Inject(method = "renderFire", at = @At("RETURN"))
	private static void rv$fireEnd(Minecraft mc, PoseStack pose, CallbackInfo ci) {
		LowFire.end(pose);
	}
	//#endif
}
