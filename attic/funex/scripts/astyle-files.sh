#!/bin/bash
# Custom wrapper-script over astyle-utility.
# Requires Artistic Style Version >= 2.04

$(which astyle &> /dev/null) || { echo "no astyle" ; exit 1; }
version=$(astyle --version 2>&1)
versnum=$(echo "$version" | awk '{print 100*$4}')
$([ "$versnum" -gt "203" ]) || { echo "$version" "-- unsupported" ; exit 2; }

astyle -n --style=1tbs \
	--indent=tab=4 \
	--convert-tabs \
	--align-pointer=name \
	--pad-oper \
	--pad-header \
	--unpad-paren \
	--min-conditional-indent=0 \
	--indent-preprocessor \
	--add-brackets \
	--break-after-logical \
	--max-code-length=80 \
	--indent-col1-comments \
	--indent-switches \
	--lineend=linux "$@" | egrep -v Unchanged

# --keep-one-line-blocks

