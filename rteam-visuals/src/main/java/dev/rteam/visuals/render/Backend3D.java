package dev.rteam.visuals.render;

//#if MC < 1.21.3
import com.mojang.blaze3d.systems.RenderSystem;
import com.mojang.blaze3d.vertex.BufferBuilder;
import com.mojang.blaze3d.vertex.DefaultVertexFormat;
import com.mojang.blaze3d.vertex.Tesselator;
//#if MC >= 1.17
import com.mojang.blaze3d.vertex.VertexFormat;
import net.minecraft.client.renderer.GameRenderer;
//#endif
//#if MC >= 1.21
import com.mojang.blaze3d.vertex.BufferUploader;
import com.mojang.blaze3d.vertex.MeshData;
//#endif
//#if MC >= 1.19.3
import org.joml.Matrix4f;
//#else
import com.mojang.math.Matrix4f;
//#endif
//#endif

//#if MC >= 1.21.3
import com.mojang.blaze3d.vertex.PoseStack;
import com.mojang.blaze3d.vertex.VertexConsumer;
import org.joml.Matrix4f;
//#endif
//#if MC >= 1.21.3 && MC < 1.21.9
import net.minecraft.client.renderer.MultiBufferSource;
import net.minecraft.client.renderer.RenderType;
//#endif
//#if MC >= 1.21.9
import net.minecraft.client.renderer.SubmitNodeCollector;
//#endif
//#if MC >= 1.21.11
import net.minecraft.client.renderer.rendertype.RenderTypes;
//#elseif MC >= 1.21.9
import net.minecraft.client.renderer.RenderType;
//#endif

/** Draws the world quads collected by {@link R3D}, once per rendering era. */
public final class Backend3D {
	private Backend3D() {
	}

	//#if MC < 1.21.3
	/** Immediate mode: the matrix already contains the camera rotation; vertices are camera relative. */
	public static void drawImmediate(Matrix4f m, QuadBuffer q) {
		RenderSystem.enableBlend();
		RenderSystem.defaultBlendFunc();
		RenderSystem.disableCull();
		RenderSystem.enableDepthTest();
		RenderSystem.depthMask(false);
		//#if MC < 1.17
		RenderSystem.disableTexture();
		RenderSystem.disableAlphaTest();
		RenderSystem.disableLighting();
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
		RenderSystem.setShader(GameRenderer::getPositionColorShader);
		BufferBuilder bb = Tesselator.getInstance().begin(VertexFormat.Mode.QUADS, DefaultVertexFormat.POSITION_COLOR);
		//#endif
		for (int i = 0; i < q.vertices(); i++) {
			int c = q.color(i);
			//#if MC >= 1.21
			bb.addVertex(m, q.x(i), q.y(i), q.z(i)).setColor(c);
			//#else
			bb.vertex(m, q.x(i), q.y(i), q.z(i)).color((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, (c >>> 24) & 0xFF).endVertex();
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
		RenderSystem.depthMask(true);
		RenderSystem.enableCull();
		RenderSystem.disableBlend();
	}
	//#endif

	//#if MC >= 1.21.3 && MC < 1.21.9
	/** Into the entity buffer source of the main pass (drawn by vanilla right after). */
	public static void drawToBuffer(PoseStack ps, MultiBufferSource source, QuadBuffer q) {
		VertexConsumer vc = source.getBuffer(RenderType.debugQuads());
		Matrix4f m = ps.last().pose();
		for (int i = 0; i < q.vertices(); i++) {
			vc.addVertex(m, q.x(i), q.y(i), q.z(i)).setColor(q.color(i));
		}
	}
	//#endif

	//#if MC >= 1.21.9
	/** As custom geometry of the frame's submit queue (1.21.9+). */
	public static void submit(PoseStack ps, SubmitNodeCollector collector, QuadBuffer q) {
		final QuadBuffer copy = q.copy();
		//#if MC >= 1.21.11
		collector.submitCustomGeometry(ps, RenderTypes.debugQuads(), (pose, vc) -> emit(pose, vc, copy));
		//#else
		collector.submitCustomGeometry(ps, RenderType.debugQuads(), (pose, vc) -> emit(pose, vc, copy));
		//#endif
	}

	private static void emit(PoseStack.Pose pose, VertexConsumer vc, QuadBuffer q) {
		for (int i = 0; i < q.vertices(); i++) {
			vc.addVertex(pose, q.x(i), q.y(i), q.z(i)).setColor(q.color(i));
		}
	}
	//#endif
}
