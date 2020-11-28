#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <thread.h>

#define ROOT2 1.05946309436
#define RATE 44100;

Image *back;
int rate;
double tones[16];
double *buf[2];
double ab = 0.0;
Rectangle A;
Rectangle B;

void
redraw(Image *screen)
{
	Rectangle r, dot, b;
	int dur = rate/4;
	int w, h;
	int i;

	draw(screen, screen->r, back, nil, ZP);

	// balance
	r = screen->r;
	r.max.y = r.min.y + 30;
	draw(screen, insetrect(r, 3), display->white, nil, ZP);
	r.min.x += Dx(screen->r) * (ab + 1.0)/2.0 - 1;
	r.max.x = screen->r.min.x + Dx(screen->r) * (ab + 1.0)/2.0 + 1;
	draw(screen, r, display->black, nil, ZP);

	// A
	r = screen->r;
	r.min.y += 30;
	r.max.y -= 30;
	r.max.x -= Dx(screen->r)/2;
	A = insetrect(r, 3);
	w = Dx(A);
	h = Dy(A);
	draw(screen, A, display->white, nil, ZP);
	for(i = 0; i < dur; i++){
		dot = rectaddpt(Rect((int)(((double)i/dur) * w),
							(int)(((buf[0][i] + 1.0)/2.0) * h),
							(int)(((double)(i+1)/dur) * w),
							(int)(((buf[0][i] + 1.0)/2.0) * h) + 1),
				A.min);
		draw(screen, dot, display->black, nil, ZP);
	}

	for(i = 0; i < 3; i++){
		r = screen->r;
		r.min.y = r.max.y - 30;
		r.max.x = r.min.x + Dx(r)/6;
		b = insetrect(rectaddpt(r, Pt(Dx(r)*i, 0)), 3);
		draw(screen, b, display->white, nil, ZP);
	}

	// B
	r = screen->r;
	r.min.y += 30;
	r.max.y -= 30;
	r.min.x += Dx(screen->r)/2;
	B = insetrect(r, 3);
	w = Dx(B);
	h = Dy(B);
	draw(screen, B, display->white, nil, ZP);
	for(i = 0; i < dur; i++){
		dot = rectaddpt(Rect((int)(((double)i/dur) * w),
							(int)(((buf[1][i] + 1.0)/2.0) * h),
							(int)(((double)(i+1)/dur) * w),
							(int)(((buf[1][i] + 1.0)/2.0) * h) + 1),
				B.min);
		draw(screen, dot, display->black, nil, ZP);
	}

	for(i = 0; i < 3; i++){
		r = screen->r;
		r.min.y = r.max.y - 30;
		r.max.x = r.min.x + Dx(r)/6;
		b = insetrect(rectaddpt(r, Pt(Dx(r)*(i+3), 0)), 3);
		draw(screen, b, display->white, nil, ZP);
	}

	flushimage(display, 1);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		fprint(2,"can't reattach to window");
	redraw(screen);
}

void
audioproc(void *unused)
{
	int i, j, step, dur;
	double tone, sum;
	short out;
	char *buffer;
	char str[6*16+2];
	int k;
	int fd = open("/mnt/monome/buttons", OREAD);

	if (fd < 0)
		sysfatal("open /mnt/monome/buttons: %r");

	dur = rate/4;
	buffer = malloc(dur * 2);

	for(;;){
		for(j = 0; j < 16; j++){
			if (readn(fd, str, 6*16+2) != (6*16+2))
				sysfatal("readn /mnt/monome/buttons: %r");
			k = atoi(&str[j*6]);
			for(step = 0; step < dur; step++){
				sum = 0.0;
				for(i = 0; i < 16; i++){
					if(k & (1<<i)){
						tone = tones[16 - i - 1];
						sum += buf[0][(int)(step*(tone/4)) % dur] * (ab - 1.0) * -0.5;
						sum += buf[1][(int)(step*(tone/4)) % dur] * (ab + 1.0) *  0.5;
					}
				}

				out = (short)(sum * 2047.0 * pow(0.9999, step/10.0));
				buffer[step*2+0] = out & 0xFF;
				buffer[step*2+1] = (out >> 8) & 0xFF;
			}
			write(1, buffer, dur*2);
		}
	}

	free(buffer);
}

void
threadmain(int argc, char **argv)
{
	int i, j;
	int dur;
	Mouse m;
	Point xy;
	int dx;

	rate = RATE;
	if(argc > 1)
		rate = atoi(argv[1]);

	dur = rate/4;

	if(initdraw(0, 0, "waves") < 0)
		sysfatal("initdraw: %r");
	einit(Emouse);

	back = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x777777FF);
	if(back == nil)
		sysfatal("allocimage: %r");

	buf[0] = malloc(dur * sizeof(double));
	buf[1] = malloc(dur * sizeof(double));

	for(i = 0; i < dur; i++)
		buf[0][i] = buf[1][i] = cos(((double)i/dur)*2*PI);

	for(i = 0; i < 16; i++){
		tones[i] = 220.0;
		for(j = 0; j < i; j++)
			tones[i] *= ROOT2;
	}

	eresized(0);
	proccreate(audioproc, nil, 8192*8);

	for(;;m = emouse()){
		if(m.buttons & 1){
			xy = subpt(m.xy, screen->r.min);
			if (xy.y < 30) {
				ab = (double)xy.x / Dx(screen->r);
				ab -= 0.5;
				ab *= 2.0;
			}
			else if ((screen->r.max.y - m.xy.y) < 30){
				dx = Dx(screen->r)/6;
				if (xy.x < dx)
					for(i = 0; i < dur; i++)
						buf[0][i] = cos(((double)i/dur)*2*PI);
				else if (xy.x < 2*dx) {
					for(i = 0; i < dur; i++)
						buf[0][i] = 1.0;
					for(i = dur/4; i < 3*dur/4; i++)
						buf[0][i] = -1.0;
				}
				else if (xy.x < 3*dx) {
					for(i = 0; i < dur/2; i++)
						buf[0][i] = -((double)i/(dur/2)*2.0-1.0);
					for(i = dur/2; i < dur; i++)
						buf[0][i] = -(0.75-((double)i/dur))*4.0;
				}
				else if (xy.x < 4*dx)
					for(i = 0; i < dur; i++)
						buf[1][i] = cos(((double)i/dur)*2*PI);
				else if (xy.x < 5*dx) {
					for(i = 0; i < dur; i++)
						buf[1][i] = 1.0;
					for(i = dur/4; i < 3*dur/4; i++)
						buf[1][i] = -1.0;
				}
				else if (xy.x < 6*dx) {
					for(i = 0; i < dur/2; i++)
						buf[1][i] = -((double)i/(dur/2)*2.0-1.0);
					for(i = dur/2; i < dur; i++)
						buf[1][i] = -(0.75-((double)i/dur))*4.0;
				}
			}
			else if(ptinrect(m.xy, A)) {
				xy = subpt(m.xy, A.min);
				dx = Dx(A);
				buf[0][(int)(((double)xy.x/dx)*dur)] = (((double)xy.y/Dy(A))-0.5)*2.0;
			}
			else if(ptinrect(m.xy, B)) {
				xy = subpt(m.xy, B.min);
				dx = Dx(B);
				buf[1][(int)(((double)xy.x/dx)*dur)] = (((double)xy.y/Dy(B))-0.5)*2.0;
			}
			redraw(screen);
		}
	}
}
