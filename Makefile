.PHONY: all clean

all: manual_control.h gamepad_control.h

manual_control.h: html/manual_control.html tools/gzip_to_header.py
	python3 tools/gzip_to_header.py html/manual_control.html manual_control.h

gamepad_control.h: html/gamepad_control.html tools/gzip_to_header.py
	python3 tools/gzip_to_header.py html/gamepad_control.html gamepad_control.h

clean:
	rm -f manual_control.h gamepad_control.h
