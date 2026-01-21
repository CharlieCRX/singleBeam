# ======================================================
# Makefile for singleBeam
# 生成静态库(.a)和动态库(.so)，仅暴露 core/sbeam.h
# ======================================================

# ======================================================
# 目标板配置
# ======================================================
# TARGET_IP     ?= 10.1.2.190
TARGET_IP     ?= 192.168.1.8
TARGET_USER   ?= root
TARGET_PASSWD ?= 1
TARGET_PATH   ?= /usr/bin/
SSHPASS       := sshpass -p $(TARGET_PASSWD)

# ======================================================
# 编译器配置
# ======================================================
CC      := /home/crx/work/rk3568SDK/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc
ifeq ($(origin CC), default)
#CC := aarch64-poky-linux-gcc
endif
AR      := /home/crx/work/rk3568SDK/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-ar
CFLAGS  := -Wall -O2 -fPIC -Icore -Idev -Iprotocol -Iapp -Iutils
LDFLAGS := -lpthread -lrt -lm

# ======================================================
# 路径与文件组织
# ======================================================
BUILD_DIR := build/makefile

CORE_SRC      := $(wildcard core/*.c)
DEV_SRC       := $(wildcard dev/*.c)
PROTOCOL_SRC  := $(wildcard protocol/*.c)
APP_SRC       := $(wildcard app/*.c)

CORE_OBJ      := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_SRC))
DEV_OBJ       := $(patsubst %.c,$(BUILD_DIR)/%.o,$(DEV_SRC))
PROTOCOL_OBJ  := $(patsubst %.c,$(BUILD_DIR)/%.o,$(PROTOCOL_SRC))
APP_OBJ       := $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_SRC))

# ======================================================
# 输出文件
# ======================================================
STATIC_LIB  := $(BUILD_DIR)/sBeam.a
SHARED_LIB  := $(BUILD_DIR)/sBeam.so

TARGETS     := \
	$(BUILD_DIR)/ad5932_main \
	$(BUILD_DIR)/dac63001_main \
	$(BUILD_DIR)/i2c_fpga_test \
	$(BUILD_DIR)/fpga_udp_test \
	$(BUILD_DIR)/ad8338_gain_sweep_with_resistors \
	$(BUILD_DIR)/sbeam_test \
	$(BUILD_DIR)/test_lib_sbeam

# ======================================================
# 默认目标
# ======================================================
all: $(STATIC_LIB) $(SHARED_LIB) $(TARGETS)

# ======================================================
# 构建静态库和动态库
# ======================================================
$(STATIC_LIB): $(CORE_OBJ) $(DEV_OBJ) $(PROTOCOL_OBJ)
	@echo "→ 生成静态库 $@"
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(CORE_OBJ) $(DEV_OBJ) $(PROTOCOL_OBJ)
	@echo "→ 生成动态库 $@"
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# ======================================================
# 可执行文件（使用静态库或动态库）
# ======================================================
$(BUILD_DIR)/sbeam_test: $(BUILD_DIR)/app/sbeam_test.o $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/ad5932_main: $(BUILD_DIR)/app/ad5932_main.o $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/dac63001_main: $(BUILD_DIR)/app/dac63001_main.o $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/i2c_fpga_test: $(BUILD_DIR)/app/i2c_fpga.o $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/fpga_udp_test: $(BUILD_DIR)/app/fpga_udp_test.o $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/ad8338_gain_sweep_with_resistors: $(BUILD_DIR)/app/ad8338_gain_sweep_with_resistors.o $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ======================================================
# 库调用测试程序（使用静态库）
# ======================================================
$(BUILD_DIR)/test_lib_sbeam: app/test_lib_sbeam.c $(STATIC_LIB)
	@echo "→ 构建库调用测试程序（使用静态库）$@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ app/test_lib_sbeam.c $(STATIC_LIB) $(LDFLAGS)

# ======================================================
# 通用编译规则
# ======================================================
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/core $(BUILD_DIR)/dev $(BUILD_DIR)/protocol $(BUILD_DIR)/app

# ======================================================
# 清理与安装
# ======================================================
clean:
	rm -rf $(BUILD_DIR)

install: all
	@echo "==== 上传可执行文件到目标板 $(TARGET_IP):$(TARGET_PATH) ===="
	@$(SSHPASS) ssh -o StrictHostKeyChecking=no $(TARGET_USER)@$(TARGET_IP) "mkdir -p $(TARGET_PATH)"
	@for exe in $(TARGETS); do \
		echo "→ 上传可执行文件 $$(basename $$exe)"; \
		$(SSHPASS) scp -o StrictHostKeyChecking=no "$$exe" $(TARGET_USER)@$(TARGET_IP):$(TARGET_PATH)/; \
	done
	@echo "==== 上传完成 ===="

# ======================================================
# 调试信息
# ======================================================
print-vars:
	@echo "CORE_SRC: $(CORE_SRC)"
	@echo "DEV_SRC: $(DEV_SRC)"
	@echo "PROTOCOL_SRC: $(PROTOCOL_SRC)"
	@echo "APP_SRC: $(APP_SRC)"
	@echo "STATIC_LIB: $(STATIC_LIB)"
	@echo "SHARED_LIB: $(SHARED_LIB)"
	@echo "TARGETS: $(TARGETS)"

# ======================================================
# 专门用于库测试的目标
# ======================================================
.PHONY: lib-test
lib-test: $(BUILD_DIR)/test_lib_sbeam
	@echo "✅ 库调用测试程序已构建（使用静态库）"

.PHONY: all clean install print-vars lib-test
