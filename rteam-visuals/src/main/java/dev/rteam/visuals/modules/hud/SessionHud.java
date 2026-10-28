package dev.rteam.visuals.modules.hud;

import dev.rteam.visuals.api.Mc;
import dev.rteam.visuals.core.Theme;
import dev.rteam.visuals.core.module.HudModule;
import dev.rteam.visuals.render.R2D;
import dev.rteam.visuals.util.Lang;

/** How long you have been playing on this server. */
public final class SessionHud extends HudModule {
	private long joined = System.currentTimeMillis();
	private String server = "";

	public SessionHud() {
		super("session", Lang.of("Session", "Сессия"), Lang.of("Play time on the current server", "Время игры на текущем сервере"), 0.99f, 0.98f);
	}

	@Override
	public void onWorldJoin() {
		joined = System.currentTimeMillis();
		server = Mc.serverAddress();
	}

	@Override
	protected void draw(float pt, boolean editor) {
		long s = (System.currentTimeMillis() - joined) / 1000L;
		String time = String.format("%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
		String name = Mc.player() != null ? Mc.player().getName().getString() : "Player";
		String srv = server.isEmpty() ? "-" : server;
		float w = Math.max(110, Math.max(R2D.width(srv), R2D.width(name)) + 50);
		float h = 46;
		HudStyle.accentPanel(0, 0, w, h);
		HudStyle.header(Lang.t("Session", "Сессия"), 0, 0, w);
		R2D.text(Lang.t("Time", "Время"), 6, 16, Theme.textDim());
		R2D.text(time, w - 6 - R2D.width(time), 16, Theme.text());
		R2D.text(Lang.t("Player", "Игрок"), 6, 26, Theme.textDim());
		R2D.text(name, w - 6 - R2D.width(name), 26, Theme.text());
		R2D.text(Lang.t("Server", "Сервер"), 6, 36, Theme.textDim());
		R2D.text(srv, w - 6 - R2D.width(srv), 36, Theme.text());
		width = w;
		height = h;
	}
}
