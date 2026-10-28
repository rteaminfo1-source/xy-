package dev.rteam.visuals.render;

import dev.rteam.visuals.api.Mc;
import net.minecraft.world.item.ItemStack;

//#if MC >= 26.1
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.navigation.ScreenRectangle;
//#elseif MC >= 1.21.6
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.navigation.ScreenRectangle;
import dev.rteam.visuals.mixin.GuiGraphicsAccessor;
//#elseif MC >= 1.20
import net.minecraft.client.gui.GuiGraphics;
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.Tesselator;
import com.mojang.blaze3d.vertex.VertexFormat;
import org.joml.Matrix4f;
//#else
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.Tesselator;
//#endif

//#if MC >= 1.17 && MC < 1.20
import com.mojang.blaze3d.vertex.VertexFormat;
import net.minecraft.client.renderer.GameRenderer;
//#endif
//#if MC >= 1.19.3 && MC < 1.20
import org.joml.Matrix4f;
//#elseif MC < 1.19.3
import com.mojang.math.Matrix4f;
//#endif
//#if MC >= 1.20 && MC < 1.21.2
import net.minecraft.client.renderer.GameRenderer;
//#endif
//#if MC >= 1.21.2 && MC < 1.21.5
import net.minecraft.client.renderer.CoreShaders;
//#endif
//#if MC >= 1.21 && MC < 1.21.5
import com.mojang.blaze3d.vertex.BufferUploader;
import com.mojang.blaze3d.vertex.MeshData;
//#endif
//#if MC >= 1.21.5 && MC < 1.21.6
import com.mojang.blaze3d.vertex.VertexConsumer;
import net.minecraft.client.renderer.RenderType;
//#endif

/**
 * The part of the 2D renderer that talks to the game. One implementation per rendering era:
 * <ul>
 * <li>1.16.5: immediate mode with fixed-function GL state</li>
 * <li>1.17 - 1.21.4: immediate mode with the position_color shader</li>
 * <li>1.21.5: the GUI buffer source</li>
 * <li>1.21.6+: our own GUI element in the deferred GUI render state</li>
 * </ul>
 */
public final class Backend2D {
	private Backend2D() {
	}

	static void begin(Object ctx) {
	}

	static void end(Object ctx) {
	}

	static void nextLayer(Object ctx) {
		//#if MC >= 1.21.6
		g(ctx).nextStratum();
		//#endif
	}

	//#if MC >= 26.1
	private static GuiGraphicsExtractor g(Object ctx) {
		return (GuiGraphicsExtractor) ctx;
	}
	//#elseif MC >= 1.20
	private static GuiGraphics g(Object ctx) {
		return (GuiGraphics) ctx;
	}
	//#else
	private static PoseStack g(Object ctx) {
		return (PoseStack) ctx;
	}
	//#endif

	// ---- quads ----------------------------------------------------------------------------------

	static void drawQuads(Object ctx, QuadBuffer q, float[] clip) {
		//#if MC >= 1.21.6
		RvGuiElement.submit(ctx, q, clip);
		//#elseif MC >= 1.21.5
		GuiGraphics gg = g(ctx);
		final Matrix4f m = gg.pose().last().pose();
		gg.drawSpecial(source -> {
			VertexConsumer vc = source.getBuffer(RenderType.gui());
			for (int i = 0; i < q.vertices(); i++) {
				vc.addVertex(m, q.x(i), q.y(i), 0f).setColor(q.color(i));
			}
		});
		gg.flush();
		//#else
		//#if MC >= 1.20
		GuiGraphics gg = g(ctx);
		gg.flush();
		Matrix4f m = gg.pose().last().pose();
		//#else
		Matrix4f m = g(ctx).last().pose();
		//#endif
		RenderSystem.enableBlend();
		RenderSystem.defaultBlendFunc();
		RenderSystem.disableCull();
		//#if MC < 1.17
		RenderSystem.disableTexture();
		RenderSystem.disableAlphaTest();
		RenderSystem.shadeModel(7425);
		BufferBuilder bb = Tesselator.getInstance().getBuilder();
		bb.begin(7, DefaultVertexFormat.POSITION_COLOR);
		//#elseif MC < 1.21
		//#if MC < 1.19.4
		RenderSystem.disableTexture();
		//#endif
		RenderSystem.setShader(GameRenderer::getPositionColorShader);
		BufferBuilder bb = Tesselator.getInstance().getBuilder();
		bb.begin(VertexFormat.Mode.QUADS, DefaultVertexFormat.POSITION_COLOR);
		//#else
		//#if MC >= 1.21.2
		RenderSystem.setShader(CoreShaders.POSITION_COLOR);
		//#else
		RenderSystem.setShader(GameRenderer::getPositionColorShader);
		//#endif
		BufferBuilder bb = Tesselator.getInstance().begin(VertexFormat.Mode.QUADS, DefaultVertexFormat.POSITION_COLOR);
		//#endif
		for (int i = 0; i < q.vertices(); i++) {
			int c = q.color(i);
			//#if MC >= 1.21
			bb.addVertex(m, q.x(i), q.y(i), 0f).setColor(c);
			//#else
			bb.vertex(m, q.x(i), q.y(i), 0f).color((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, (c >>> 24) & 0xFF).endVertex();
			//#endif
		}
		//#if MC >= 1.21
		MeshData mesh = bb.build();
		if (mesh != null) BufferUploader.drawWithShader(mesh);
		//#else
		Tesselator.getInstance().end();
		//#endif
		//#if MC < 1.17
		RenderSystem.shadeModel(7424);
		RenderSystem.enableAlphaTest();
		RenderSystem.enableTexture();
		//#elseif MC < 1.19.4
		RenderSystem.enableTexture();
		//#endif
		RenderSystem.enableCull();
		RenderSystem.disableBlend();
		//#endif
	}

	// ---- clipping -------------------------------------------------------------------------------

	static void setClip(Object ctx, float[] clip) {
		//#if MC >= 1.20
		// GuiGraphics keeps a scissor stack: R2D pushes and pops in the same order
		//#else
		if (clip == null) {
			RenderSystem.disableScissor();
			return;
		}
		double s = Mc.guiScale();
		int fbH = Mc.pixelHeight();
		int x = (int) Math.floor(clip[0] * s);
		int y = (int) Math.floor(fbH - clip[3] * s);
		int w = Math.max(0, (int) Math.ceil((clip[2] - clip[0]) * s));
		int h = Math.max(0, (int) Math.ceil((clip[3] - clip[1]) * s));
		RenderSystem.enableScissor(x, y, w, h);
		//#endif
	}

	static void pushClip(Object ctx, float[] clip) {
		//#if MC >= 1.20
		g(ctx).enableScissor((int) Math.floor(clip[0]), (int) Math.floor(clip[1]), (int) Math.ceil(clip[2]), (int) Math.ceil(clip[3]));
		//#else
		setClip(ctx, clip);
		//#endif
	}

	static void popClip(Object ctx, float[] parent) {
		//#if MC >= 1.20
		g(ctx).disableScissor();
		//#else
		setClip(ctx, parent);
		//#endif
	}

	// ---- text -----------------------------------------------------------------------------------

	static void text(Object ctx, String s, float x, float y, float sx, float sy, int color, boolean shadow) {
		//#if MC >= 1.21.6
		g(ctx).pose().pushMatrix();
		g(ctx).pose().translate(x, y);
		g(ctx).pose().scale(sx, sy);
		//#if MC >= 26.1
		g(ctx).text(Mc.font(), s, 0, 0, color, shadow);
		//#else
		g(ctx).drawString(Mc.font(), s, 0, 0, color, shadow);
		//#endif
		g(ctx).pose().popMatrix();
		//#elseif MC >= 1.20
		g(ctx).pose().pushPose();
		g(ctx).pose().translate(x, y, 0f);
		g(ctx).pose().scale(sx, sy, 1f);
		g(ctx).drawString(Mc.font(), s, 0, 0, color, shadow);
		g(ctx).pose().popPose();
		//#else
		PoseStack ps = g(ctx);
		ps.pushPose();
		ps.translate(x, y, 0);
		ps.scale(sx, sy, 1f);
		if (shadow) Mc.font().drawShadow(ps, s, 0f, 0f, color);
		else Mc.font().draw(ps, s, 0f, 0f, color);
		ps.popPose();
		//#endif
	}

	// ---- items ----------------------------------------------------------------------------------

	static void item(Object ctx, ItemStack stack, float x, float y, float scale, boolean decorations) {
		//#if MC >= 1.21.6
		g(ctx).pose().pushMatrix();
		g(ctx).pose().translate(x, y);
		g(ctx).pose().scale(scale, scale);
		//#if MC >= 26.1
		g(ctx).item(stack, 0, 0);
		if (decorations) g(ctx).itemDecorations(Mc.font(), stack, 0, 0);
		//#else
		g(ctx).renderItem(stack, 0, 0);
		if (decorations) g(ctx).renderItemDecorations(Mc.font(), stack, 0, 0);
		//#endif
		g(ctx).pose().popMatrix();
		//#elseif MC >= 1.20
		g(ctx).pose().pushPose();
		g(ctx).pose().translate(x, y, 0f);
		g(ctx).pose().scale(scale, scale, 1f);
		g(ctx).renderItem(stack, 0, 0);
		if (decorations) g(ctx).renderItemDecorations(Mc.font(), stack, 0, 0);
		g(ctx).pose().popPose();
		//#elseif MC >= 1.19.4
		PoseStack ps = g(ctx);
		ps.pushPose();
		ps.translate(x, y, 0f);
		ps.scale(scale, scale, 1f);
		Mc.mc().getItemRenderer().renderAndDecorateItem(ps, stack, 0, 0);
		if (decorations) Mc.mc().getItemRenderer().renderGuiItemDecorations(ps, Mc.font(), stack, 0, 0);
		ps.popPose();
		//#elseif MC >= 1.17
		PoseStack mv = RenderSystem.getModelViewStack();
		mv.pushPose();
		mv.translate(x, y, 0);
		mv.scale(scale, scale, 1f);
		RenderSystem.applyModelViewMatrix();
		Mc.mc().getItemRenderer().renderAndDecorateItem(stack, 0, 0);
		if (decorations) Mc.mc().getItemRenderer().renderGuiItemDecorations(Mc.font(), stack, 0, 0);
		mv.popPose();
		RenderSystem.applyModelViewMatrix();
		//#else
		RenderSystem.pushMatrix();
		RenderSystem.translatef(x, y, 0f);
		RenderSystem.scalef(scale, scale, 1f);
		Mc.mc().getItemRenderer().renderAndDecorateItem(stack, 0, 0);
		if (decorations) Mc.mc().getItemRenderer().renderGuiItemDecorations(Mc.font(), stack, 0, 0);
		RenderSystem.popMatrix();
		//#endif
	}

	//#if MC >= 1.21.6
	static ScreenRectangle rect(float[] r) {
		if (r == null) return null;
		int x = (int) Math.floor(r[0]);
		int y = (int) Math.floor(r[1]);
		return new ScreenRectangle(x, y, Math.max(0, (int) Math.ceil(r[2]) - x), Math.max(0, (int) Math.ceil(r[3]) - y));
	}
	//#endif
}
