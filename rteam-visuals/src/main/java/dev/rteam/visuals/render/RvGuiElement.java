package dev.rteam.visuals.render;

//#if MC >= 1.21.6
import com.mojang.blaze3d.vertex.VertexConsumer;
import dev.rteam.visuals.mixin.GuiGraphicsAccessor;
import net.minecraft.client.gui.navigation.ScreenRectangle;
import net.minecraft.client.gui.render.TextureSetup;
import net.minecraft.client.renderer.RenderPipelines;
import org.joml.Matrix3x2f;
//#if MC >= 26.3
import com.mojang.renderpearl.api.pipeline.RenderPipeline;
//#else
import com.mojang.blaze3d.pipeline.RenderPipeline;
//#endif
//#if MC >= 26.1
import net.minecraft.client.renderer.state.gui.GuiElementRenderState;
import net.minecraft.client.renderer.state.gui.GuiRenderState;
//#else
import net.minecraft.client.gui.render.state.GuiElementRenderState;
import net.minecraft.client.gui.render.state.GuiRenderState;
//#endif

/** Our colored quads as one element of the deferred GUI render state (1.21.6+). */
public final class RvGuiElement implements GuiElementRenderState {
	private static final Matrix3x2f IDENTITY = new Matrix3x2f();

	private final QuadBuffer quads;
	private final ScreenRectangle scissor;
	private final ScreenRectangle bounds;

	private RvGuiElement(QuadBuffer quads, ScreenRectangle scissor, ScreenRectangle bounds) {
		this.quads = quads;
		this.scissor = scissor;
		this.bounds = bounds;
	}

	static void submit(Object graphics, QuadBuffer q, float[] clip) {
		ScreenRectangle sc = Backend2D.rect(clip);
		int x = (int) Math.floor(q.minX);
		int y = (int) Math.floor(q.minY);
		ScreenRectangle b = new ScreenRectangle(x, y, Math.max(1, (int) Math.ceil(q.maxX) - x), Math.max(1, (int) Math.ceil(q.maxY) - y));
		if (sc != null) {
			b = b.intersection(sc);
			if (b == null) return;
		}
		GuiRenderState state = ((GuiGraphicsAccessor) graphics).rv$guiRenderState();
		state.submitGuiElement(new RvGuiElement(q.copy(), sc, b));
	}

	//#if MC >= 1.21.9
	@Override
	public void buildVertices(VertexConsumer vc) {
		for (int i = 0; i < quads.vertices(); i++) {
			vc.addVertexWith2DPose(IDENTITY, quads.x(i), quads.y(i)).setColor(quads.color(i));
		}
	}
	//#else
	@Override
	public void buildVertices(VertexConsumer vc, float z) {
		for (int i = 0; i < quads.vertices(); i++) {
			vc.addVertexWith2DPose(IDENTITY, quads.x(i), quads.y(i), z).setColor(quads.color(i));
		}
	}
	//#endif

	@Override
	public RenderPipeline pipeline() {
		return RenderPipelines.GUI;
	}

	@Override
	public TextureSetup textureSetup() {
		return TextureSetup.noTexture();
	}

	@Override
	public ScreenRectangle scissorArea() {
		return scissor;
	}

	@Override
	public ScreenRectangle bounds() {
		return bounds;
	}
}
//#endif
