package dev.rteam.visuals.mixin;

//#if MC >= 1.21.6
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;
//#if MC >= 26.1
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.renderer.state.gui.GuiRenderState;
//#else
import net.minecraft.client.gui.GuiGraphics;
import net.minecraft.client.gui.render.state.GuiRenderState;
//#endif

//#if MC >= 26.1
@Mixin(GuiGraphicsExtractor.class)
//#else
@Mixin(GuiGraphics.class)
//#endif
public interface GuiGraphicsAccessor {
	@Accessor("guiRenderState")
	GuiRenderState rv$guiRenderState();
}
//#endif
