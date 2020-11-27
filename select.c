#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

Image *back;
int fd;
Rectangle buttons[8][2];
char saved[8];
char **states;

void
redraw(Image *screen)
{
	int i;

	draw(screen, screen->r, back, nil, ZP);

	for(i = 0; i < 8; i++){
		draw(screen, buttons[i][0], saved[i]? display->white: display->black, nil, ZP);
		draw(screen, buttons[i][1], display->black, nil, ZP);
	}

	flushimage(display, 1);
}

void
resize(Image *screen)
{
	int s, w, h;
	int x, y;

	x = screen->r.min.x;
	y = screen->r.min.y;

	w = Dx(screen->r)/8;
	h = Dy(screen->r)/2;

	for (s = 0; s < 8; s++) {
		buttons[s][0] = insetrect(Rect(x + w*s, y, x + w*(s+1), y+h), 3);
		buttons[s][1] = insetrect(Rect(x + w*s, y+h, x + w*(s+1), y+2*h), 3);
	}
}

void
save(int i)
{
	if (readn(fd, states[i], 6*16+2) != (6*16+2))
		sysfatal("readn: %r");

	saved[i] = 1;
}

void
restore(int i)
{
	if (saved[i] == 0)
		return;

	write(fd, states[i], 6*16+2);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		fprint(2,"can't reattach to window");
	resize(screen);
	redraw(screen);
}

void
main(int argc, char **argv)
{
	Mouse m;
	Point xy;
	int i;

	fd = open("/mnt/monome/buttons", ORDWR);
	if(fd < 0)
		sysfatal("open /mnt/monome/buttons: %r");

	if(initdraw(0, 0, "select") < 0)
		sysfatal("initdraw: %r");

	back = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x777777FF);
	if (back == nil)
		sysfatal("allocimage: %r");

	states = malloc(8 * sizeof(char*));
	for(i = 0; i < 8; i++)
		states[i] = calloc(1, 6*16+2);
	memset(saved, '\0', 8);

	einit(Emouse);
	resize(screen);
	redraw(screen);

	for(;;m = emouse()){
		if(m.buttons & 1){
			for (i = 0; i < 8; i++) {
				if (ptinrect(m.xy, buttons[i][0]))
					save(i);
				if (ptinrect(m.xy, buttons[i][1]))
					restore(i);
			}
			redraw(screen);
		}
	}	
}
