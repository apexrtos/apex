SECTIONS {
	. = 0x100;
	.text . : {
		*(.text)
		__etext = . ;
	}
}
