# Host-side unit tests for hardware-independent firmware code.
#   make         build and run all tests (with address and undefined-behaviour sanitizers)

CXX      ?= g++
APP       = ../Drumboy-Pro-App-H723/Core/Library
CXXFLAGS  = -std=gnu++14 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I$(APP)/Global
BUILD    ?= build

TESTS = test_fill test_routing test_beatlist test_euclid

.PHONY: all clean
all: $(addprefix $(BUILD)/,$(TESTS))
	@for t in $^; do echo "== $$t"; ./$$t || exit 1; done

$(BUILD)/%: %.cpp $(wildcard $(APP)/Global/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)
