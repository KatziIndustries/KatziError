build-linux:
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

	gcc -c shared/message.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/message.o

	gcc -c linux/xdg-shell-protocol.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/xdg-shell-protocol.o

	ar rcs libkerror.a \
		build/*.o

	gcc test/main.c -L. -lkerror \
		-L/usr/local/lib \
		-lwayland-client \
		-lffi \
		-lX11 \
		-lxcb \
		-lXau \
		-lXdmcp \
		-o app

build-linux-shared:
	mkdir -p build

	gcc -fPIC -c linux/main.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/main.o

	gcc -fPIC -c linux/x11.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/x11.o

	gcc -fPIC -c linux/wayland.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/wayland.o

	gcc -fPIC -c shared/message.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/message.o

	gcc -fPIC -c linux/xdg-shell-protocol.c \
		$(pkg-config --cflags wayland-client) \
		-I./include \
		-o build/xdg-shell-protocol.o

	gcc -shared -o libkerror.so build/*.o \
		-lwayland-client \
		-lffi \
		-lX11 \
		-lxcb \
		-lXau \
		-lXdmcp

build-windows:
	mkdir -p build

	gcc -c windows/main.c \
		-I./include \
		-o build/main.o

	gcc -c windows/native.c \
		-I./include \
		-o build/native.o

	ar rcs libkerror.a \
		build/*.o

run:
	./app

clean:
	rm -f app
	rm -fr build
