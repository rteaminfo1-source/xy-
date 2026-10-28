package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Anim;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;
import net.minecraft.client.KeyMapping;

import java.util.ArrayDeque;
import java.util.Deque;

/** WASD, mouse buttons with CPS and space. */
public final class Keystrokes extends HudModule {
	private final BoolSetting mouse = add(new BoolSetting("mouse", Lang.of("Mouse buttons", "Кнопки мыши"), true));
	private final BoolSetting space = add(new BoolSetting("space", Lang.of("Space", "Пробел"), true));
	private final BoolSetting cps = add(new BoolSetting("cps", Lang.of("CPS", "CPS (клики/сек)"), true));

	private final Anim[] anims = new Anim[7];
	private final Deque<Long> left = new ArrayDeque<>();
	private final Deque<Long> right = new ArrayDeque<>();
	private boolean wasLeft, wasRight;

	public Keystrokes() {
		super("keystrokes", Lang.of("Keystrokes", "Клавиши"), Lang.of("Pressed keys and clicks per second", "Нажатые клавиши и клики в секунду"), 0.01f, 0.6f);
		for (int i = 0; i < anims.length; i++) anims[i] = new Anim(0f, 120, Anim.Easing.OUT_QUAD);
	}

	private static int count(Deque<Long> q) {
		long now = System.currentTimeMillis();
		while (!q.isEmpty() && now - q.peekFirst() > 1000L) q.pollFirst();
		return q.size();
	}

	private void key(int i, String label, float x, float y, float w, float h, boolean down) {
		anims[i].to(down ? 1f : 0f);
		float p = anims[i].get();
		R2D.roundRect(x, y, w, h, 4, ColorUtil.lerp(Theme.hudBg(), ColorUtil.withAlpha(Theme.accent1, 200), p));
		if (p > 0.01f) R2D.roundOutline(x, y, w, h, 4, 1f, ColorUtil.fade(Theme.accent2, p));
		int tc = ColorUtil.lerp(Theme.text(), 0xFFFFFFFF, p);
		R2D.text(label, x + w / 2f - R2D.width(label) / 2f, y + h / 2f - 4, tc);
	}

	@Override
	protected void draw(float pt, boolean editor) {
		KeyMapping[] k = {Mc.mc().options.keyUp, Mc.mc().options.keyLeft, Mc.mc().options.keyDown, Mc.mc().options.keyRight};
		boolean l = Mc.mc().options.keyAttack.isDown(), r = Mc.mc().options.keyUse.isDown();
		if (l && !wasLeft) left.addLast(System.currentTimeMillis());
		if (r && !wasRight) right.addLast(System.currentTimeMillis());
		wasLeft = l;
		wasRight = r;
		float s = 20, g = 2;
		float x0 = 0;
		key(0, "W", x0 + s + g, 0, s, s, k[0].isDown());
		key(1, "A", x0, s + g, s, s, k[1].isDown());
		key(2, "S", x0 + s + g, s + g, s, s, k[2].isDown());
		key(3, "D", x0 + (s + g) * 2, s + g, s, s, k[3].isDown());
		float y = (s + g) * 2;
		float fullW = s * 3 + g * 2;
		if (mouse.on()) {
			float bw = (fullW - g) / 2f;
			key(4, cps.on() ? "LMB " + count(left) : "LMB", x0, y, bw, s, l);
			key(5, cps.on() ? "RMB " + count(right) : "RMB", x0 + bw + g, y, bw, s, r);
			y += s + g;
		}
		if (space.on()) {
			anims[6].to(Mc.mc().options.keyJump.isDown() ? 1f : 0f);
			float p = anims[6].get();
			R2D.roundRect(x0, y, fullW, 10, 4, ColorUtil.lerp(Theme.hudBg(), ColorUtil.withAlpha(Theme.accent1, 200), p));
			R2D.roundRect(x0 + fullW / 2f - 12, y + 4.5f, 24, 1.2f, 0.6f, ColorUtil.lerp(Theme.textDim(), 0xFFFFFFFF, p));
			y += 10;
		}
		width = fullW;
		height = y;
	}
}
