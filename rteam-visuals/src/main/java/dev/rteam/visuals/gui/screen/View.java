package dev.rteam.visuals.gui.screen;

/** Version independent screen contents. {@link RvScreen} adapts it to Minecraft's Screen API. */
public interface View {
	void init(int width, int height);

	/** Called inside R2D.begin/end. */
	void render(int mouseX, int mouseY, float delta);

	boolean mouseClicked(double x, double y, int button);

	boolean mouseReleased(double x, double y, int button);

	boolean mouseDragged(double x, double y, int button, double dx, double dy);

	boolean mouseScrolled(double x, double y, double amount);

	boolean keyPressed(int key, int modifiers);

	boolean charTyped(String text);

	/** Escape / close requested: return true to close now, false if the view animates out first. */
	boolean requestClose();

	void removed();

	void tick();

	/** Whether to draw the vanilla (blurred) background behind the view. */
	boolean vanillaBackground();
}
