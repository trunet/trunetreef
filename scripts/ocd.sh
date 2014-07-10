#!/bin/bash

openocd -c "gdb_memory_map enable" \
	-c "gdb_flash_program enable" \
	-c "reset_config trst_and_srst" \
	-f /usr/local/share/openocd/scripts/interface/olimex-arm-usb-ocd.cfg \
	-f /usr/local/share/openocd/scripts/target/stm32f1x.cfg \
	#-c "log_output openocd.log" \
