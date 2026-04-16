HTML_SRCS := $(wildcard html/*.html)
HTML_HDRS := $(HTML_SRCS:.html=.h)

.PHONY: all clean

all: $(HTML_HDRS)

html/%.h: html/%.html tools/gzip_to_header.py
	python3 tools/gzip_to_header.py $< $@

clean:
	rm -f $(HTML_HDRS)
