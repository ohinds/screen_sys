default: screen_sys

screen_sys: screen_sys.cpp
	g++ screen_sys.cpp -o screen_sys

install:	screen_sys
	install screen_sys /usr/bin/
