-include $(top_builddir)/common.mk

EXTRA_DIST = completion/cautac-completion.bash

bashcompletiondir = $(datarootdir)/bash-completion/completions

dist_bashcompletion_DATA = cautac

cautac: completion/cautac-completion.bash
	@cp -f $< $@

