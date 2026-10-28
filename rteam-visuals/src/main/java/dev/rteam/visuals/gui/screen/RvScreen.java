package dev.rteam.visuals.gui.screen;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.render.R2D;
import net.minecraft.client.gui.screens.Screen;
//#if MC >= 26.1
import net.minecraft.client.gui.GuiGraphicsExtractor;
//#elseif MC >= 1.20
import net.minecraft.client.gui.GuiGraphics;
//#else
import com.mojang.blaze3d.vertex.PoseStack;
//#endif
//#if MC >= 1.21.9
import net.minecraft.client.input.CharacterEvent;
import net.minecraft.client.input.KeyEvent;
import net.minecraft.client.input.MouseButtonEvent;
//#endif

/** The only class that knows Minecraft's Screen API; everything else is a {@link View}. */
public final class RvScreen extends Screen {
	private final View view;
	private boolean textInput;

	public RvScreen(View view) {
		super(Mc.literal("RTeam Visuals"));
		this.view = view;
	}

	public View view() {
		return view;
	}

	@Override
	protected void init() {
		view.init(this.width, this.height);
	}

	@Override
	public boolean isPauseScreen() {
		return false;
	}

	@Override
	public boolean shouldCloseOnEsc() {
		return true;
	}

	@Override
	public void onClose() {
		if (view.requestClose()) closeNow();
	}

	/** Closes without asking the view again (after its closing animation). */
	public void closeNow() {
		setTextInput(false);
		super.onClose();
	}

	@Override
	public void removed() {
		setTextInput(false);
		view.removed();
		super.removed();
	}

	@Override
	public void tick() {
		view.tick();
	}

	/** Tells the game that a text field is focused (needed for typing on 26.3+ with SDL input). */
	public void setTextInput(boolean on) {
		if (textInput == on) return;
		textInput = on;
		//#if MC >= 26.1
		try {
			Mc.mc().onTextInputFocusChange(this, on);
		} catch (RuntimeException ignored) {
		}
		//#endif
	}

	// ---- rendering ------------------------------------------------------------------------------

	//#if MC >= 26.1
	@Override
	public void extractRenderState(GuiGraphicsExtractor graphics, int mouseX, int mouseY, float delta) {
		draw(graphics, mouseX, mouseY, delta);
	}

	@Override
	public void extractBackground(GuiGraphicsExtractor graphics, int mouseX, int mouseY, float delta) {
		if (view.vanillaBackground()) super.extractBackground(graphics, mouseX, mouseY, delta);
	}
	//#elseif MC >= 1.20
	@Override
	public void render(GuiGraphics graphics, int mouseX, int mouseY, float delta) {
		//#if MC >= 1.20.2 && MC < 1.21.6
		if (view.vanillaBackground()) super.renderBackground(graphics, mouseX, mouseY, delta);
		//#endif
		draw(graphics, mouseX, mouseY, delta);
	}

	//#if MC >= 1.20.2
	@Override
	public void renderBackground(GuiGraphics graphics, int mouseX, int mouseY, float delta) {
		//#if MC >= 1.21.6
		if (view.vanillaBackground()) super.renderBackground(graphics, mouseX, mouseY, delta);
		//#endif
	}
	//#endif
	//#else
	@Override
	public void render(PoseStack pose, int mouseX, int mouseY, float delta) {
		draw(pose, mouseX, mouseY, delta);
	}
	//#endif

	private void draw(Object graphics, int mouseX, int mouseY, float delta) {
		R2D.begin(graphics);
		try {
			view.render(mouseX, mouseY, delta);
		} finally {
			R2D.end();
		}
	}

	// ---- input ----------------------------------------------------------------------------------

	//#if MC >= 1.21.9
	@Override
	public boolean mouseClicked(MouseButtonEvent event, boolean doubleClick) {
		return view.mouseClicked(event.x(), event.y(), event.button()) || super.mouseClicked(event, doubleClick);
	}

	@Override
	public boolean mouseReleased(MouseButtonEvent event) {
		return view.mouseReleased(event.x(), event.y(), event.button()) || super.mouseReleased(event);
	}

	@Override
	public boolean mouseDragged(MouseButtonEvent event, double dx, double dy) {
		return view.mouseDragged(event.x(), event.y(), event.button(), dx, dy) || super.mouseDragged(event, dx, dy);
	}

	@Override
	public boolean keyPressed(KeyEvent event) {
		if (view.keyPressed(event.key(), event.modifiers())) return true;
		return super.keyPressed(event);
	}

	@Override
	public boolean charTyped(CharacterEvent event) {
		return view.charTyped(new String(Character.toChars(event.codepoint())));
	}
	//#else
	@Override
	public boolean mouseClicked(double x, double y, int button) {
		return view.mouseClicked(x, y, button) || super.mouseClicked(x, y, button);
	}

	@Override
	public boolean mouseReleased(double x, double y, int button) {
		return view.mouseReleased(x, y, button) || super.mouseReleased(x, y, button);
	}

	@Override
	public boolean mouseDragged(double x, double y, int button, double dx, double dy) {
		return view.mouseDragged(x, y, button, dx, dy) || super.mouseDragged(x, y, button, dx, dy);
	}

	@Override
	public boolean keyPressed(int key, int scancode, int modifiers) {
		if (view.keyPressed(key, modifiers)) return true;
		return super.keyPressed(key, scancode, modifiers);
	}

	@Override
	public boolean charTyped(char c, int modifiers) {
		return view.charTyped(String.valueOf(c));
	}
	//#endif

	//#if MC >= 1.20.2
	@Override
	public boolean mouseScrolled(double x, double y, double horizontal, double vertical) {
		return view.mouseScrolled(x, y, vertical);
	}
	//#else
	@Override
	public boolean mouseScrolled(double x, double y, double amount) {
		return view.mouseScrolled(x, y, amount);
	}
	//#endif
}
