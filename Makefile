BUILD_DIR := build

.PHONY: configure editor game viewer test clean

configure:
	cmake -S . -B $(BUILD_DIR)

editor: configure
	cmake --build $(BUILD_DIR) --target level-editor --parallel
	./$(BUILD_DIR)/apps/level_editor/level-editor

game: configure
	cmake --build $(BUILD_DIR) --target pixel-roguelike --parallel
	./$(BUILD_DIR)/apps/runtime/pixel-roguelike

viewer: configure
	cmake --build $(BUILD_DIR) --target procedural-model-viewer --parallel
	./$(BUILD_DIR)/apps/model_viewer/procedural-model-viewer

test: configure
	cmake --build $(BUILD_DIR) --parallel
	cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	rm -rf $(BUILD_DIR)
