package dev.rteam.visuals.mixin;

import dev.rteam.visuals.render.Backend3D;
import dev.rteam.visuals.render.QuadBuffer;
import dev.rteam.visuals.render.R3D;
import net.minecraft.client.renderer.LevelRenderer;
import net.minecraft.world.phys.Vec3;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
//#if MC >= 1.21.9
import com.mojang.blaze3d.vertex.PoseStack;
import dev.rteam.visuals.api.Mc;
import net.minecraft.client.renderer.SubmitNodeCollector;
//#if MC >= 26.1
import net.minecraft.client.renderer.state.level.LevelRenderState;
//#else
import net.minecraft.client.renderer.state.LevelRenderState;
//#endif
//#elseif MC >= 1.21.3
import com.mojang.blaze3d.vertex.PoseStack;
import net.minecraft.client.Camera;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.world.entity.Entity;
import java.util.List;
//#elseif MC >= 1.20.5
import net.minecraft.client.Camera;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.LightTexture;
import org.joml.Matrix4f;
//#if MC >= 1.21
import net.minecraft.client.DeltaTracker;
//#endif
//#else
import com.mojang.blaze3d.vertex.PoseStack;
import net.minecraft.client.Camera;
import net.minecraft.client.renderer.GameRenderer;
import net.minecraft.client.renderer.LightTexture;
//#if MC >= 1.19.3
import org.joml.Matrix4f;
//#else
import com.mojang.math.Matrix4f;
//#endif
//#endif

/** World effects (china hat, circles, trails, target marker): drawn at the right point of each version's level rendering. */
@Mixin(LevelRenderer.class)
public class LevelRendererMixin {
	//#if MC >= 1.21.9
	@Inject(method = "submitEntities", at = @At("TAIL"))
	private void rv$world(PoseStack pose, LevelRenderState state, SubmitNodeCollector collector, CallbackInfo ci) {
		Vec3 cam = state.cameraRenderState.pos;
		QuadBuffer q = R3D.collect(cam.x, cam.y, cam.z, Mc.partialTick());
		if (q != null) Backend3D.submit(pose, collector, q);
	}
	//#elseif MC >= 1.21.3
	@Inject(method = "renderEntities", at = @At("TAIL"))
	private void rv$world(PoseStack pose, MultiBufferSource.BufferSource source, Camera camera, DeltaTracker delta, List<Entity> entities, CallbackInfo ci) {
		//#if MC >= 1.21.8
		Vec3 cam = camera.position();
		//#else
		Vec3 cam = camera.getPosition();
		//#endif
		QuadBuffer q = R3D.collect(cam.x, cam.y, cam.z, delta.getGameTimeDeltaPartialTick(false));
		if (q != null) Backend3D.drawToBuffer(pose, source, q);
	}
	//#elseif MC >= 1.21
	@Inject(method = "renderLevel", at = @At("RETURN"))
	private void rv$world(DeltaTracker delta, boolean blockOutline, Camera camera, GameRenderer gameRenderer, LightTexture light, Matrix4f view, Matrix4f projection, CallbackInfo ci) {
		Vec3 cam = camera.getPosition();
		QuadBuffer q = R3D.collect(cam.x, cam.y, cam.z, delta.getGameTimeDeltaPartialTick(false));
		if (q != null) Backend3D.drawImmediate(new Matrix4f(view), q);
	}
	//#elseif MC >= 1.20.5
	@Inject(method = "renderLevel", at = @At("RETURN"))
	private void rv$world(float partialTick, long nanos, boolean blockOutline, Camera camera, GameRenderer gameRenderer, LightTexture light, Matrix4f view, Matrix4f projection, CallbackInfo ci) {
		Vec3 cam = camera.getPosition();
		QuadBuffer q = R3D.collect(cam.x, cam.y, cam.z, partialTick);
		if (q != null) Backend3D.drawImmediate(new Matrix4f(view), q);
	}
	//#else
	@Inject(method = "renderLevel", at = @At("RETURN"))
	private void rv$world(PoseStack pose, float partialTick, long nanos, boolean blockOutline, Camera camera, GameRenderer gameRenderer, LightTexture light, Matrix4f projection, CallbackInfo ci) {
		Vec3 cam = camera.getPosition();
		QuadBuffer q = R3D.collect(cam.x, cam.y, cam.z, partialTick);
		if (q != null) Backend3D.drawImmediate(pose.last().pose(), q);
	}
	//#endif
}
