package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.Hooks;
import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.core.setting.ModeSetting;
import dev.rteam.visuals.core.setting.TextSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.ColorUtil;
import dev.rteam.visuals.util.Lang;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

public final class Watermark extends HudModule {
	private final ModeSetting style = add(new ModeSetting("style", Lang.of("Style", "Стиль"), 0,
			new String[]{"pill", "card", "text"},
			new Lang[]{Lang.of("Pill", "Капсула"), Lang.of("Card", "Карточка"), Lang.of("Text", "Текст")}));
	private final TextSetting title = add(new TextSetting("title", Lang.of("Title", "Название"), "RTeam Visuals", 24));
	private final BoolSetting showFps = add(new BoolSetting("fps", Lang.of("FPS", "FPS"), true));
	private final BoolSetting showPing = add(new BoolSetting("ping", Lang.of("Ping", "Пинг"), true));
	private final BoolSetting showTime = add(new BoolSetting("time", Lang.of("Time", "Время"), true));
	private final BoolSetting showServer = add(new BoolSetting("server", Lang.of("Server", "Сервер"), false));

	private static final SimpleDateFormat CLOCK = new SimpleDateFormat("HH:mm");

	public Watermark() {
		super("watermark", Lang.of("Watermark", "Ватермарка"), Lang.of("Client logo with FPS, ping and time", "Логотип клиента с FPS, пингом и временем"), 0.01f, 0.01f);
		setEnabled(true, false);
	}

	private List<String> infos() {
		List<String> out = new ArrayList<>();
		if (showFps.on()) out.add(Hooks.fps() + " fps");
		if (showPing.on() && Mc.inGame()) out.add(Mc.ping() + " ms");
		if (showTime.on()) out.add(CLOCK.format(new Date()));
		if (showServer.on() && Mc.inGame()) out.add(Mc.serverAddress());
		return out;
	}

	@Override
	protected void draw(float pt, boolean editor) {
		String name = title.get().isEmpty() ? "RTeam Visuals" : title.get();
		List<String> info = infos();
		float phase = (System.currentTimeMillis() % 4000L) / 4000f;
		if (style.is("text")) {
			R2D.textGradient(name, 2, 2, Theme.accent1, Theme.accent2, true, 1.35f, phase);
			float y = 2 + 12;
			StringBuilder sb = new StringBuilder();
			for (String s : info) sb.append(sb.length() == 0 ? "" : "  ·  ").append(s);
			R2D.textShadow(sb.toString(), 2, y + 1, Theme.textDim());
			width = Math.max(R2D.width(name) * 1.35f, R2D.width(sb.toString())) + 4;
			height = y + 11;
			return;
		}
		if (style.is("card")) {
			StringBuilder sb = new StringBuilder();
			for (String s : info) sb.append(sb.length() == 0 ? "" : "  ·  ").append(s);
			float w = Math.max(R2D.width(name) * 1.2f + 30, R2D.width(sb.toString()) + 30);
			float h = 30;
			HudStyle.panel(0, 0, w, h);
			logo(4, 5, 20);
			R2D.textGradient(name, 28, 5, Theme.accent1, Theme.accent2, false, 1.2f, phase);
			R2D.text(sb.toString(), 28, 18, Theme.textDim());
			width = w;
			height = h;
			return;
		}
		// pill
		float h = 16;
		float x = 0;
		float w = 20 + R2D.width(name) + 6;
		for (String s : info) w += R2D.width(s) + 11;
		HudStyle.panel(0, 0, w, h);
		logo(3, 2, 12);
		x = 19;
		R2D.textGradient(name, x, 4, Theme.accent1, Theme.accent2, false, 1f, phase);
		x += R2D.width(name) + 5;
		for (String s : info) {
			R2D.rect(x, 4, x + 1, h - 4, Theme.stroke());
			x += 6;
			R2D.text(s, x, 4, Theme.text());
			x += R2D.width(s) + 5;
		}
		width = w;
		height = h;
	}

	/** Round gradient badge with "R". */
	private static void logo(float x, float y, float size) {
		R2D.roundRect4(x, y, size, size, size * 0.3f, Theme.accent1, Theme.gradient(0.3f), Theme.accent2, Theme.gradient(0.7f));
		float s = size / 12f;
		R2D.text("R", x + size / 2f - R2D.width("R") * s / 2f, y + size / 2f - 4 * s, ColorUtil.withAlpha(0xFFFFFF, 255), false, s);
	}
}
