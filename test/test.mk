check: test/32/run test/64/run

$(APEX_SUBDIR)test/32/run: test/32/test
	$(APEX_SUBDIR)test/32/test

$(APEX_SUBDIR)test/64/run: test/64/test
	$(APEX_SUBDIR)test/64/test

MK := 32/test.mk 64/test.mk
