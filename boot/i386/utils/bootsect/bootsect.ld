ENTRY(_boot)
SECTIONS {
	. = 0x7C00;
	.text . : {
		*(.text)
		__etext = . ;
	}
}
