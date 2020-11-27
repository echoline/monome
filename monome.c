#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <fcall.h>
#include <9p.h>

int mainstacksize = 0x10000;

Image *back;
Rectangle buttons[16][16];
char buttonstate[16][16];
int redrawfds[2];

void
redraw(Image *screen)
{
	int i, j;

	draw(screen, screen->r, back, nil, ZP);

	for(i = 0; i < 16; i++)
		for(j = 0; j < 16; j++)
			draw(screen, rectaddpt(buttons[i][j], screen->r.min),
				buttonstate[i][j]? display->white: display->black,
				nil, ZP);
}

void
resize(void)
{
	int fd = open("/dev/wctl", OWRITE);
	if(fd >= 0){
		fprint(fd, "resize -dx 650 -dy 650\n");
		close(fd);
	}
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		fprint(2,"can't reattach to window");
	resize();
	redraw(screen);
}

void
fswrite(Req *r)
{
	int i, j, k;
	int count;

	count = r->ifcall.count;

	if (count < (6*16)) {
		respond(r, "short write");
		return;
	}

	for(i = 0; i < 16; i++){
		k = atoi(&r->ifcall.data[i*6]);

		for(j = 0; j < 16; j++){
			buttonstate[i][j] = (k & (1<<j))? 1: 0;
		}
	}

	write(redrawfds[1], "\0", 1);

	r->ofcall.count = count;
	respond(r, nil);
}

void
fsread(Req *r)
{
	int i, j, k;
	int count, off;
	char buf[6*16+2];

	count = r->ifcall.count;
	off = r->ifcall.offset % (6*16+2);

	if (count == 0) {
		r->ofcall.count = 0;
		respond(r, nil);
		return;
	}

	for(i = 0; i < 16; i++){
		k = 0;
		for(j = 0; j < 16; j++){
			k |= buttonstate[i][j] << j;
		}

		sprint(&buf[i*6], "%5d ", k);
	}

	buf[6*16+0] = '\n';
	buf[6*16+1] = '\0';

	if (count > (6*16+2))
		count = 6*16+2;

	memcpy(r->ofcall.data, buf + off, count);
	r->ofcall.count = count;

	respond(r, nil);
}

Srv fs = {
	.write	= fswrite,
	.read	= fsread,
};

void
threadmain(int argc, char **argv)
{
	int i, j;
	Event e;
	Point xy;
	int key;

	if(initdraw(0, 0, "monome") < 0)
		sysfatal("initdraw: %r");

	back = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x777777FF);
	if (back == nil)
		sysfatal("allocimage: %r");
	for(i = 0; i < 16; i++) {
		memset(buttonstate[i], '\0', 16);
		for(j = 0; j < 16; j++)
			buttons[i][j] = insetrect(Rect(i*40, j*40, (i+1)*40, (j+1)*40), 3);
	}
	eresized(0);
	einit(Emouse|Ekeyboard);
	pipe(redrawfds);
	estart(0, redrawfds[0], 1);

	fs.tree = alloctree(nil, nil, DMDIR|0777, nil);
	createfile(fs.tree->root, "buttons", nil, 0666, nil);
	threadpostmountsrv(&fs, "monome", "/mnt/monome", 0);

	for(;;){
READ:
		redraw(screen);
		switch(event(&e)){
		case Emouse:
			if(e.mouse.buttons & 1){
				xy = subpt(e.mouse.xy, screen->r.min);
				for(i = 0; i < 16; i++)
					for(j = 0; j < 16; j++)
						if(ptinrect(xy, buttons[i][j])){
							buttonstate[i][j] = !buttonstate[i][j];
							goto READ;
						}
			}
			break;
		case Ekeyboard:
			if(e.kbdc == Kdel)
				threadexitsall(nil);
			break;
		}
	}
}
