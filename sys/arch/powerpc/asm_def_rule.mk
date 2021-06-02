define fn_asm_def_rule
    # fn_asm_def_rule

    $$(eval $$(call fn_process_sources))
    $(tgt): $$($(tgt)_OBJS)
	sed '/.*#__OUT__/!d; s///; s/\#//2' $$^ > $$@
endef
