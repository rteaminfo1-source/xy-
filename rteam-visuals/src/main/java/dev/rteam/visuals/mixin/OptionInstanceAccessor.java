package dev.rteam.visuals.mixin;

//#if MC >= 1.19
import net.minecraft.client.OptionInstance;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** Raw access to an option's value without the range check (used to raise gamma temporarily). */
@Mixin(OptionInstance.class)
public interface OptionInstanceAccessor {
	@Accessor("value")
	Object rv$getValue();

	@Accessor("value")
	void rv$setValue(Object value);
}
//#endif
