#!/bin/sh
#
# Pre-commit hook:
# 1) Adds an empty line at the end of the file
# 2) Removes trailing spaces
#
# References:
# http://eng.wealthfront.com/2011/03/corrective-action-with-gits-pre-commit.html
# http://stackoverflow.com/questions/591923/make-git-automatically-remove-trailing-whitespace-before-committing
#

print() { echo "$0: $*" >&2; }

# Files (not deleted) in the index
files=$(git diff-index --name-status --cached HEAD | grep -v ^D | cut -c3-)
if [ "$files" != "" ]
then
	for f in $files
	do
		# Add a line-break to the file if it doesn't have one
		# (only for known text files)
		if [[ "$f" =~ [.](c|h|cpp|hpp|conf|html|log|properties|txt|xml|am|sh|md)$ ]]
		then
			# Add a line-break to the file if it doesn't have one
                        lln=$(tail -c1 "$f" | sed -e 's/[[:space:]]*$//')
			if [ -n "${lln}" ]
			then
				print "Add line-break:" "$f"
				echo >> "$f"
				git add "$f"
			fi
		fi

		# Remove trailing whitespace if it exists
		if grep -q "[[:blank:]]$" "$f"
		then
			print "Remove trailing spaces:" "$f"
			sed -i 's/[[:space:]]*$//' "$f"
			git add "$f"
		fi
	done
fi

exit 0
