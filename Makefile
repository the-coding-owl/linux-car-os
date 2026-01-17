all:
	mkdir -p bin
	g++ src/main.cpp -o bin/CarOS `pkg-config --cflags --libs gtk4 libgpiodcxx gstreamer-1.0` -lcurl -pthread