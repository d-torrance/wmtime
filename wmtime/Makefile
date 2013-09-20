LIBDIR = -L/usr/X11R6/lib
LIBS   = -lXpm -lXext -lX11 -lm
OBJS =	wmtime.o \
		../wmgeneral/wmgeneral.o \
		../wmgeneral/misc.o \
		../wmgeneral/list.o

.c.o:
	cc -c -O2 -Wall $< -o $*.o

wmtime: $(OBJS)
	cc -o wmtime $^ $(LIBDIR) $(LIBS)

clean::
	for i in $(OBJS) ; do \
		rm $$i; \
	done
	rm wmtime
