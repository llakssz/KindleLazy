# i used amazon's own toolchain, from kindle fw sources
CROSS_COMPILE = ~/opt/cross-gcc-linaro/bin/arm-linux-gnueabi-
CFLAGS = -std=c++0x
LDFLAGS = -lXtst -lX11
SRC = kindlelazy.cpp jsoncpp.cpp
OUT = kindlelazy

# -print-search-dirs

kindle: $(SRC)
	$(CROSS_COMPILE)g++ -DBUILD_KINDLE $(SRC) -o $(OUT) $(CFLAGS) $(LDFLAGS)

host: $(SRC)
	g++ -DBUILD_KINDLE $(SRC) -o $(OUT) $(CFLAGS) $(LDFLAGS)

clean:
	rm $(OUT)