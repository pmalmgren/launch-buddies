all: build

build: lb
	gcc -fno-omit-frame-pointer lb.c -o lb

clean:
	rm lb
