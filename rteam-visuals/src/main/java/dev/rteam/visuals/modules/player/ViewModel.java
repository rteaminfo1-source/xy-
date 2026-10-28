package dev.rteam.visuals.modules.player;

import com.mojang.blaze3d.vertex.PoseStack;
import dev.rteam.visuals.core.module.Category;
import dev.rteam.visuals.core.module.Module;
import dev.rteam.visuals.core.setting.NumberSetting;
import dev.rteam.visuals.util.Lang;
//#if MC >= 1.19.3
import com.mojang.math.Axis;
//#else
import com.mojang.math.Vector3f;
//#endif

/** Position, size and rotation of held items. */
public final class ViewModel extends Module {
	private final NumberSetting x = add(new NumberSetting("x", Lang.of("Offset X", "Сдвиг X"), 0.0, -1.0, 1.0, 0.02));
	private final NumberSetting y = add(new NumberSetting("y", Lang.of("Offset Y", "Сдвиг Y"), 0.0, -1.0, 1.0, 0.02));
	private final NumberSetting z = add(new NumberSetting("z", Lang.of("Offset Z", "Сдвиг Z"), 0.0, -1.0, 1.0, 0.02));
	private final NumberSetting scale = add(new NumberSetting("scale", Lang.of("Scale", "Размер"), 1.0, 0.3, 1.5, 0.05));
	private final NumberSetting rx = add(new NumberSetting("rx", Lang.of("Rotation X", "Поворот X"), 0.0, -180.0, 180.0, 1.0));
	private final NumberSetting ry = add(new NumberSetting("ry", Lang.of("Rotation Y", "Поворот Y"), 0.0, -180.0, 180.0, 1.0));
	private final NumberSetting rz = add(new NumberSetting("rz", Lang.of("Rotation Z", "Поворот Z"), 0.0, -180.0, 180.0, 1.0));

	private static ViewModel instance;

	public ViewModel() {
		super("viewmodel", Lang.of("View Model", "Вид рук"), Lang.of("Move, scale and rotate held items", "Положение, размер и поворот предметов в руках"), Category.PLAYER);
		instance = this;
	}

	public static void apply(PoseStack pose, boolean rightHand) {
		ViewModel vm = instance;
		if (vm == null || !vm.isEnabled()) return;
		float side = rightHand ? 1f : -1f;
		pose.translate(vm.x.f() * side, vm.y.f(), vm.z.f());
		//#if MC >= 26.3
		pose.rotate(Axis.XP.rotationDegrees(vm.rx.f()));
		pose.rotate(Axis.YP.rotationDegrees(vm.ry.f() * side));
		pose.rotate(Axis.ZP.rotationDegrees(vm.rz.f() * side));
		//#elseif MC >= 1.19.3
		pose.mulPose(Axis.XP.rotationDegrees(vm.rx.f()));
		pose.mulPose(Axis.YP.rotationDegrees(vm.ry.f() * side));
		pose.mulPose(Axis.ZP.rotationDegrees(vm.rz.f() * side));
		//#else
		pose.mulPose(Vector3f.XP.rotationDegrees(vm.rx.f()));
		pose.mulPose(Vector3f.YP.rotationDegrees(vm.ry.f() * side));
		pose.mulPose(Vector3f.ZP.rotationDegrees(vm.rz.f() * side));
		//#endif
		float s = vm.scale.f();
		pose.scale(s, s, s);
	}
}
