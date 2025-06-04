.PHONY: all clean

all:
	cmake --preset Release . && cmake --build . --preset Release

clean:
	rm -r build || true