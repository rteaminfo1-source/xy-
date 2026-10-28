package dev.rteam.visuals;

import dev.rteam.visuals.core.config.Config;
import dev.rteam.visuals.core.module.ModuleManager;
import dev.rteam.visuals.modules.hud.ArmorHud;
import dev.rteam.visuals.modules.hud.CoordsHud;
import dev.rteam.visuals.modules.hud.InfoHud;
import dev.rteam.visuals.modules.hud.InventoryHud;
import dev.rteam.visuals.modules.hud.KeybindsHud;
import dev.rteam.visuals.modules.hud.Keystrokes;
import dev.rteam.visuals.modules.hud.ModuleList;
import dev.rteam.visuals.modules.hud.NotificationsModule;
import dev.rteam.visuals.modules.hud.PotionHud;
import dev.rteam.visuals.modules.hud.SessionHud;
import dev.rteam.visuals.modules.hud.TargetHud;
import dev.rteam.visuals.modules.hud.Watermark;
import dev.rteam.visuals.modules.misc.ClickGuiModule;
import dev.rteam.visuals.modules.player.CameraTweaks;
import dev.rteam.visuals.modules.player.LowFire;
import dev.rteam.visuals.modules.player.NoHurtCam;
import dev.rteam.visuals.modules.player.SwingAnimation;
import dev.rteam.visuals.modules.player.ViewModel;
import dev.rteam.visuals.modules.player.Zoom;
import dev.rteam.visuals.modules.render.ChinaHat;
import dev.rteam.visuals.modules.render.Crosshair;
import dev.rteam.visuals.modules.render.HitParticles;
import dev.rteam.visuals.modules.render.JumpCircles;
import dev.rteam.visuals.modules.render.TargetEsp;
import dev.rteam.visuals.modules.render.Trails;
import dev.rteam.visuals.modules.world.Ambience;
import dev.rteam.visuals.modules.world.Fullbright;
import dev.rteam.visuals.modules.world.Weather;
import net.fabricmc.api.ClientModInitializer;
//#if MC >= 1.18
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
//#else
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;
//#endif

public final class RteamVisuals implements ClientModInitializer {
	public static final String MOD_ID = "rteamvisuals";
	public static final String NAME = "RTeam Visuals";
	//#if MC >= 1.18
	public static final Logger LOG = LoggerFactory.getLogger(NAME);
	//#else
	public static final Logger LOG = LogManager.getLogger(NAME);
	//#endif

	@Override
	public void onInitializeClient() {
		// HUD
		ModuleManager.register(new Watermark());
		ModuleManager.register(new ModuleList());
		ModuleManager.register(new TargetHud());
		ModuleManager.register(new PotionHud());
		ModuleManager.register(new ArmorHud());
		ModuleManager.register(new Keystrokes());
		ModuleManager.register(new CoordsHud());
		ModuleManager.register(new InfoHud());
		ModuleManager.register(new InventoryHud());
		ModuleManager.register(new KeybindsHud());
		ModuleManager.register(new SessionHud());
		ModuleManager.register(new NotificationsModule());
		// render
		ModuleManager.register(new ChinaHat());
		ModuleManager.register(new JumpCircles());
		ModuleManager.register(new Trails());
		ModuleManager.register(new TargetEsp());
		ModuleManager.register(new HitParticles());
		ModuleManager.register(new Crosshair());
		// world
		ModuleManager.register(new Fullbright());
		ModuleManager.register(new Ambience());
		ModuleManager.register(new Weather());
		// player / camera
		ModuleManager.register(new NoHurtCam());
		ModuleManager.register(new Zoom());
		ModuleManager.register(new ViewModel());
		ModuleManager.register(new SwingAnimation());
		ModuleManager.register(new LowFire());
		ModuleManager.register(new CameraTweaks());
		// client
		ModuleManager.register(new ClickGuiModule());

		Config.load();
		Runtime.getRuntime().addShutdownHook(new Thread(Config::save, "RTeam Visuals config save"));
		LOG.info("[RTeam Visuals] loaded " + ModuleManager.all().size() + " modules");
	}
}
