</$objtype/mkfile

TARG=`{ls *.c | sed 's,\.c$,,'}
PROGS=${TARG:%=%}

all:V: $PROGS

%: %.$O
	$O^l -o $target $stem.$O

%.$O: %.c
	$O^c $stem.c

clean:V:
	rm -f *.$O $PROGS
