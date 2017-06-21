
all: index.html

index.html: index.rst
	rst2html5  $? $@

.PHONY: clean
clean:
	$(RM) index.html
