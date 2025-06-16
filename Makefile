.PHONY: all clean

all:
	cmake --preset Release . && cmake --build . --preset Release

clean:
	cmake --build . --preset Release --target clean