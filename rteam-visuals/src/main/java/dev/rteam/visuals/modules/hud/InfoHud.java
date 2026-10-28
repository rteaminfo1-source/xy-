package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.Hooks;
import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.core.setting.BoolSetting;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Lang;
import dev.rteam.visuals.util.MathUtil;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/** FPS, ping, speed, server and clock. */
public final class InfoHud extends HudModule {
	private final BoolSetting fps = add(new BoolSetting("fps", Lang.of("FPS", "FPS"), true));
	private final BoolSetting ping = add(new BoolSetting("ping", Lang.of("Ping", "Пинг"), true));
	private final BoolSetting speed = add(new BoolSetting("speed", Lang.of("Speed (blocks/s)", "Скорость (блоков/с)"), true));
	private final BoolSetting server = add(new BoolSetting("server", Lang.of("Server", "Сервер"), true));
	private final BoolSetting clock = add(new BoolSetting("clock", Lang.of("Clock", "Часы"), false));

	private double lastX, lastZ;
	private double bps;

	public InfoHud() {
		super("info", Lang.of("Info", "Информация"), Lang.of("FPS, ping, speed and server", "FPS, пинг, скорость и сервер"), 0.01f, 0.85f);
	}

	@Override
	public void onTick() {
		if (Mc.player() == null) return;
		double x = Mc.player().getX(), z = Mc.player().getZ();
		double d = Math.sqrt((x - lastX) * (x - lastX) + (z - lastZ) * (z - lastZ)) * 20.0;
		bps = d > 100 ? 0 : bps * 0.6 + d * 0.4;
		lastX = x;
		lastZ = z;
	}

	@Override
	protected void draw(float pt, boolean editor) {
		List<String[]> rows = new ArrayList<>();
		if (fps.on()) rows.add(new String[]{"FPS", Integer.toString(Hooks.fps())});
		if (ping.on()) rows.add(new String[]{Lang.t("Ping", "Пинг"), Mc.ping() + " ms"});
		if (speed.on()) rows.add(new String[]{Lang.t("Speed", "Скорость"), MathUtil.format(bps, 1) + " b/s"});
		if (server.on()) rows.add(new String[]{Lang.t("Server", "Сервер"), Mc.inGame() ? Mc.serverAddress() : "-"});
		if (clock.on()) rows.add(new String[]{Lang.t("Time", "Время"), new SimpleDateFormat("HH:mm:ss").format(new Date())});
		float w = 60;
		for (String[] r : rows) w = Math.max(w, R2D.width(r[0]) + R2D.width(r[1]) + 18);
		float h = 4 + rows.size() * 10 + 2;
		HudStyle.panel(0, 0, w, h);
		float y = 4;
		for (String[] r : rows) {
			R2D.text(r[0], 5, y, Theme.textDim());
			R2D.text(r[1], w - 5 - R2D.width(r[1]), y, Theme.text());
			y += 10;
		}
		width = w;
		height = h;
	}
}
