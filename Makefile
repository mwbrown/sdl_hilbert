
BUILD_DIR=./build

.PHONY: all clean

all: $(BUILD_DIR)/ $(BUILD_DIR)/Makefile
	@make --no-print-directory -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)

$(BUILD_DIR)/:
	@mkdir -p $@

$(BUILD_DIR)/Makefile:
	@cd $(BUILD_DIR) && cmake ..
