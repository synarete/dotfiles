
all: index.html

index.html: index.rst
	rst2html --theme medium-black  $? $@

.PHONY: clean
clean:
	$(RM) index.html
