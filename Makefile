all:
	mkdir -p build

	gcc -c linux/main.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/main.o

	gcc -c linux/x11.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/x11.o

	gcc -c linux/wayland.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/wayland.o

	gcc -c linux/xdg-shell-protocol.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/xdg-shell-protocol.o

	ar rcs libkerror.a \
		build/*.o

run:
	gcc test/main.c -L. -lkerror \
		-L/usr/local/lib \
		-lwayland-client \
		-lffi \
		-lX11 \
		-lxcb \
		-lXau \
		-lXdmcp \
		-o app

	./app

clean:
	rm -f app
	rm -fr build