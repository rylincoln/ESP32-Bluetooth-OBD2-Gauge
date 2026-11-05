# Runs the firmware's hardware-free modules on this computer: make -C test
# Modules that differ by vehicle are built and run once for each variant.

SKETCH := ../sketch/VaAndCobOBD2Gauge
BUILD := build
CXXFLAGS := -std=gnu++11 -Wall -Wextra -g -I$(SKETCH) -fsanitize=address,undefined -fno-sanitize-recover=all
TESTS :=
.DEFAULT_GOAL := all

# $(1) test name, $(2) source files, $(3) extra compiler flags
define add_test
TESTS += $(1)
$(BUILD)/$(1): test.h $(2) $(wildcard $(SKETCH)/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(3) -o $$@ $(2)
endef

TABLES := test_gauge_tables.cpp $(SKETCH)/gauge_tables.cpp
SETTINGS := test_gauge_settings.cpp $(SKETCH)/gauge_settings.cpp $(SKETCH)/gauge_tables.cpp

$(eval $(call add_test,obd_reply,test_obd_reply.cpp $(SKETCH)/obd_reply.cpp))
$(eval $(call add_test,gauge_tables_t6,$(TABLES)))
$(eval $(call add_test,gauge_tables_t5,$(TABLES),-DFORD_T5))
$(eval $(call add_test,elm327_link,test_elm327_link.cpp $(SKETCH)/elm327_link.cpp))
$(eval $(call add_test,button_gesture,test_button_gesture.cpp $(SKETCH)/button_gesture.cpp))
$(eval $(call add_test,gauge_settings_t6,$(SETTINGS)))
$(eval $(call add_test,gauge_settings_t5,$(SETTINGS),-DFORD_T5))
$(eval $(call add_test,shutdown_watch,test_shutdown_watch.cpp $(SKETCH)/shutdown_watch.cpp))

.PHONY: all clean
all: $(TESTS:%=run-%)

clean:
	rm -rf $(BUILD)

run-%: $(BUILD)/%
	@echo "== $*"
	@./$<
