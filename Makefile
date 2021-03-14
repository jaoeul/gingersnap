BUILD_DIR = build
SRC_DIR   = src
CFLAGS    = -Wall -Werror -pedantic -g -O2


SOURCES = $(shell find $(SRC_DIR)/ -name "*.c" | grep -v test)
TARGET  = gingersnap
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TEST_TARGET  = test_gingersnap
TEST_SOURCES = $(shell find $(SRC_DIR)/ -name "*.c" | grep -v main)
TEST_OBJECTS = $(TEST_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

default: $(TARGET)
.SECONDEXPANSION:
$(OBJECTS) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $(CFLAGS) $<
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $^
.PHONY: default

test: $(TEST_TARGET)
.SECONDEXPANSION:
$(TEST_OBJECTS) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $(CFLAGS) $<
$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) -o $@ $(CFLAGS) $^
.PHONY: test


clean:
	rm -r ./build/*
	rm ./gingersnap
	rm ./test_gingersnap
