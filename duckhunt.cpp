// Jason Thai
// Duck Hunt

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include "ppm.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

#define MAX_DUCKS 2

#define rnd() (float)rand() / (float)RAND_MAX

extern "C"
{
#include "fonts.h"
}

//X Windows variables
Display *dpy;
Window win;
GLXContext glc;

struct timespec timeStart, timeCurrent;
struct timespec timePause;
double movementCountdown = 0.0;
double timeSpan = 0.0;
const double oobillion = 1.0 / 1e9;
const double movementRate = 1.0 / 60.0;
double timeDiff(struct timespec *start, struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) + (double)(end->tv_nsec - start->tv_nsec) * oobillion;
}
void timeCopy(struct timespec *dest, struct timespec *source)
{
    memcpy(dest, source, sizeof(struct timespec));
}

//Structures

struct Vec {
    float x, y, z;
};

struct Shape {
    float width, height;
    float radius;
    Vec center;
};

struct Duck
{
    Shape s;
    Vec velocity;
    struct timespec time;
    struct Duck *prev;
    struct Duck *next;
    Duck()
    {
	prev = NULL;
	next = NULL;
    }
};

struct Game {
    int bullets;
    Duck *duck;
    int n;
    float floor;
    int rounds;
    struct timespec duckTimer;
    Shape box1[1];
    ~Game()
    {
	delete [] duck;
    }
    Game()
    {
	duck = NULL;
	bullets = 0;
	n = 0;
	floor = 100.0;
	rounds = 1;
	for(int i = 0; i <= 2; i++)
	{
	    box1[i].width = 45;
	    box1[i].height = 35;
	    box1[i].center.x = WINDOW_WIDTH - 675;
	    box1[i].center.y = WINDOW_HEIGHT - 550;
	    //box1[i].center.x = WINDOW_HEIGHT - 550;
	    //box1[i].center.y = WINDOW_WIDTH - 50;
	    box1[i].center.z = 0;
	}
    }
};

//Function prototypes
void initXWindows(void);
void init_opengl(void);
void cleanupXWindows(void);
void check_mouse(XEvent *e, Game *game);
int check_keys(XEvent *e, Game *game);
void movement(Game *game);
void render(Game *game);
void makeDuck(Game *game);
void deleteDuck(Game *game, Duck *duck);
void check_resize(XEvent *e);

Ppmimage *backgroundImage = NULL;
GLuint backgroundTexture;
int background = 1;

int main(void)
{
    int done=0;
    srand(time(NULL));
    initXWindows();
    init_opengl();

    clock_gettime(CLOCK_REALTIME, &timePause);
    clock_gettime(CLOCK_REALTIME, &timeStart);
    //declare game object
    Game game;

    //start animation
    while(!done) {
	while(XPending(dpy)) {
	    XEvent e;
	    XNextEvent(dpy, &e);
	    check_resize(&e);
	    check_mouse(&e, &game);
	    done = check_keys(&e, &game);
	}
	clock_gettime(CLOCK_REALTIME, &timeCurrent);
	timeSpan = timeDiff(&timeStart, &timeCurrent);
	timeCopy(&timeStart, &timeCurrent);
	movementCountdown += timeSpan;
	while(movementCountdown >= movementRate)
	{
	    movement(&game);
	    movementCountdown -= movementRate;
	}
	render(&game);
	glXSwapBuffers(dpy, win);
    }
    cleanupXWindows();
    cleanup_fonts();
    return 0;
}

void set_title(void)
{
    //Set the window title bar.
    XMapWindow(dpy, win);
    XStoreName(dpy, win, "Duck Hunt");
}

void cleanupXWindows(void) {
    //do not change
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
}

void initXWindows(void) {
    //do not change
    GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    int w=WINDOW_WIDTH, h=WINDOW_HEIGHT;

    XSetWindowAttributes swa;

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
	std::cout << "\n\tcannot connect to X server\n" << std::endl;
	exit(EXIT_FAILURE);
    }
    Window root = DefaultRootWindow(dpy);
    XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
    if(vi == NULL) {
	std::cout << "\n\tno appropriate visual found\n" << std::endl;
	exit(EXIT_FAILURE);
    } 
    Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    //XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
	ButtonPress | ButtonReleaseMask |
	PointerMotionMask |
	StructureNotifyMask | SubstructureNotifyMask;
    win = XCreateWindow(dpy, root, 0, 0, w, h, 0, vi->depth,
	    InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    set_title();
    glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
}

void reshape_window(int width, int height)
{
    glViewport(0, 0, (GLint)width, (GLint)height);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);
    set_title();
}

unsigned char *buildAlphaData(Ppmimage *img) {
    // add 4th component to RGB stream...
    int a,b,c;
    unsigned char *newdata, *ptr;
    unsigned char *data = (unsigned char *)img->data;
    //newdata = (unsigned char *)malloc(img->width * img->height * 4);
    newdata = new unsigned char[img->width * img->height * 4];
    ptr = newdata;
    for (int i=0; i<img->width * img->height * 3; i+=3) {
	a = *(data+0);
	b = *(data+1);
	c = *(data+2);
	*(ptr+0) = a;
	*(ptr+1) = b;
	*(ptr+2) = c;
	//
	//get the alpha value
	//
	//original code
	//get largest color component...
	//*(ptr+3) = (unsigned char)((
	//      (int)*(ptr+0) +
	//      (int)*(ptr+1) +
	//      (int)*(ptr+2)) / 3);
	//d = a;
	//if (b >= a && b >= c) d = b;
	//if (c >= a && c >= b) d = c;
	//*(ptr+3) = d;
	//
	//new code, suggested by Chris Smith, Fall 2013
	*(ptr+3) = (a|b|c);
	//
	ptr += 4;
	data += 3;
    }
    return newdata;
}

void init_opengl(void)
{
    //OpenGL initialization
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    //Initialize matrices
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    //Set 2D mode (no perspective)
    glOrtho(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, -1, 1);

    //added for background
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    //clear the screen
    glClearColor(1.0, 1.0, 1.0, 1.0);
    backgroundImage = ppm6GetImage("./images/background.ppm");
    //
    //create opengl texture elements
    glGenTextures(1, &backgroundTexture);
    //background
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    //
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, backgroundImage->width, backgroundImage->height, 0, GL_RGB, GL_UNSIGNED_BYTE, backgroundImage->data); 
    //Set the screen background color
    //glClearColor(0.1, 0.1, 0.1, 1.0);
    glEnable(GL_TEXTURE_2D);
    initialize_fonts();
}

void makeDuck(Game *game, float x, float y)
{
    if(game->n >= MAX_DUCKS)
	return;
    struct timespec dt;
    clock_gettime(CLOCK_REALTIME, &dt);
    //double ts = timeDiff(&game->duckTimer, &dt);
    timeCopy(&game->duckTimer, &dt);
    int directionNum = rand() % 101;
    bool direction;
    if(directionNum >= 50)
	direction = true;
    else
	direction = false;
    std::cout << "makeDuck() " << x << " " << y << std::endl;
    Duck *d = new Duck;
    timeCopy(&d->time, &dt);
    d->s.center.x = x;
    d->s.center.y = y;
    d->s.center.z = 0.0;
    if(direction)
	d->velocity.x = 3.0;
    else
	d->velocity.x = -3.0;
    d->velocity.y = 3.0;
    d->velocity.z = 0.0;
    d->s.width = 50.0;
    d->s.height = 50.0;
    d->next = game->duck;
    if(game->duck != NULL)
    {
	game->duck->prev = d;
    }
    game->duck = d;
    game->n++;
}

void check_resize(XEvent *e)
{
    if(e->type != ConfigureNotify)
	return;
    XConfigureEvent xce = e->xconfigure;
    if(xce.width != WINDOW_WIDTH || xce.height != WINDOW_HEIGHT)
    {
	reshape_window(xce.width, xce.height);
    }
}

void check_mouse(XEvent *e, Game *game)
{
    int y = WINDOW_HEIGHT - e->xbutton.y;

    Duck *d = game->duck;
    Duck *saved;

    if (e->type == ButtonRelease) {
	return;
    }
    if (e->type == ButtonPress) {
	if (e->xbutton.button==1) {
	    //Left button was pressed
	    while(d)
	    {	
		if(game->bullets <= 1)
		{
		    saved = d->next;
		    deleteDuck(game, d);
		    d = saved;
		    if(game->n == 1)
		    {
			saved = d->next;
			deleteDuck(game, d);
			d = saved;
		    }
		    std::cout << "no bullets" << std::endl;
		    std::cout << "game over" << std::endl;
		    return;
		}
		if(e->xbutton.x >= d->s.center.x - d->s.width &&
			e->xbutton.x <= d->s.center.x + d->s.width &&
			y <= d->s.center.y + d->s.height &&
			y >= d->s.center.y - d->s.height)
		{
		    saved = d->next;
		    deleteDuck(game, d);
		    d = saved;
		    game->bullets--;	
		    std::cout << "shoot true" << std::endl;
		    std::cout << "bullets = " << game->bullets << std::endl;
		    return;
		}
		if(game->n == 2)
		{
		    d = d->next;
		    if(e->xbutton.x >= d->s.center.x - d->s.width &&
			    e->xbutton.x <= d->s.center.x + d->s.width &&
			    y <= d->s.center.y + d->s.height &&
			    y >= d->s.center.y - d->s.height)
		    {
			saved = d->next;
			deleteDuck(game, d);
			d = saved;
			game->bullets--;
			std::cout << "shoot true" << std::endl;
			std::cout << "bullets = " << game->bullets << std::endl;
			return;
		    }
		    else
		    {
			game->bullets--;
			std::cout << "shoot false" << std::endl;
			std::cout << "bullets = " << game->bullets << std::endl;
			return;
		    }
		}
		game->bullets--;	
		std::cout << "shoot false" << std::endl;
		std::cout << "bullets = " << game->bullets << std::endl;
		d = d->next;
	    }
	}
    }
    if (e->xbutton.button==3) {
	//Right button was pressed
	return;
    }
}

int check_keys(XEvent *e, Game *game)
{
    //Was there input from the keyboard?
    if (e->type == KeyPress) {
	int key = XLookupKeysym(&e->xkey, 0);
	if (key == XK_Escape) {
	    return 1;
	}
	//You may check other keys here.
	if(key == XK_1)
	{
	    std::cout << "ROUND " << game->rounds << std::endl;
	    game->bullets = 3;
	    makeDuck(game, rand() % (WINDOW_WIDTH - 50 - 1) + 50 + 1, game->floor + 50 + 1);
	    std::cout << "makeduck" << std::endl;
	    std::cout << "bullets = " << game->bullets << std::endl;
	}
	if(key == XK_2)
	{
	    std::cout << "ROUND " << game->rounds << std::endl;
	    game->bullets = 3;
	    makeDuck(game, rand() % (WINDOW_WIDTH - 50 - 1) + 50 + 1, game->floor + 50 + 1);
	    makeDuck(game, rand() % (WINDOW_WIDTH - 50 - 1) + 50 + 1, game->floor + 50 + 1);
	    std::cout << "2 makeduck" << std::endl;
	    std::cout << "bullets = " << game->bullets << std::endl;
	}

    }
    return 0;
}

void movement(Game *game)
{
    Duck *d = game->duck;
    struct timespec dt;
    clock_gettime(CLOCK_REALTIME, &dt);

    if (game->n <= 0)
	return;

    while(d)
    {
	double ts = timeDiff(&d->time, &dt);
	std::cout << ts << std::endl;
	if(ts > 2.5)
	{
	    Duck *saved = d->next;
	    deleteDuck(game, d);
	    d = saved;
	    std::cout << "deleteDuck" << std::endl;
	    continue;
	}

	if(d->s.center.x - d->s.width <= 0.0)
	{
	    d->s.center.x = d->s.width + 0.1;
	    d->velocity.x *= -1.0;
	    std::cout << " off screen - left" << std::endl;
	}
	if(d->s.center.x + d->s.width >= WINDOW_WIDTH)
	{
	    d->s.center.x = WINDOW_WIDTH - d->s.width - 0.1;
	    d->velocity.x *= -1.0;
	    std::cout << " off screen - right" << std::endl;
	}
	if(d->s.center.y - d->s.height <= game->floor)
	{
	    d->s.center.y = game->floor + d->s.height + 0.1;
	    d->velocity.y *= -1.0;
	    std::cout << " off screen - down" << std::endl;
	}
	if(d->s.center.y + d->s.height >= WINDOW_HEIGHT)
	{
	    d->s.center.y = WINDOW_HEIGHT - d->s.height - 0.1;
	    d->velocity.y *= -1.0;
	    std::cout << " off screen - up" << std::endl;
	}

	d->s.center.x += d->velocity.x;
	d->s.center.y += d->velocity.y;

	d = d->next;	
    }
}

void render(Game *game)
{
    float w, h, x, y;

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    //WINDOW HEIGHT??
    //draw a quad with texture
    //float wid = 120.0f;
    //glColor3f(1.0,1.0,1.0);
    if(background) {
	glBindTexture(GL_TEXTURE_2D, backgroundTexture);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
	glTexCoord2f(0.0f, 0.0f); glVertex2i(0, WINDOW_HEIGHT);
	glTexCoord2f(1.0f, 0.0f); glVertex2i(WINDOW_WIDTH, WINDOW_HEIGHT);
	glTexCoord2f(1.0f, 1.0f); glVertex2i(WINDOW_WIDTH, 0);
	glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    //Drawing Shapes
    glColor3ub(255, 255, 255);
    glBegin(GL_LINES);
    glVertex2f(0.0, game->floor);
    glVertex2f(WINDOW_WIDTH, game->floor);
    glEnd();

    //GERARDO
    //Printing text in Boxes
    Rect r;
    //  glClear(GL_COLOR_BUFFER_BIT);
    r.bot = WINDOW_HEIGHT - 550;
    r.left = WINDOW_WIDTH - 715;
    r.center = 0;

    //Drawing Boxes
    Shape *s;
    //glColor3ub(90,140,90);

    //left box with Bullets     
    for (int i = 0; i<1; i++) {
	//glColor3ub(90,140,90);
	s = &game->box1[i];
	glPushMatrix();
	glTranslatef(s->center.x, s->center.y, s->center.z);
	w = s->width;
	h = s->height;

	glBegin(GL_QUADS);
	glVertex2f(-w, -h);
	glVertex2f(-w,  h);
	glVertex2f( w,  h);
	glVertex2f( w, -h);
	glEnd();
	glPopMatrix();
	ggprint16(&r, 16, 0x00ff0000, "SHOT: ");
	//ggprint16(&r, 16, 0x0099FF, "SHOT: ");
	r.bot = WINDOW_HEIGHT - 575;
	r.left = WINDOW_WIDTH - 685;
	//game->bullets = 3;
	int a = game->bullets;
	if(a == 3)
	    ggprint16(&r, 16, 0x00ff0000, "3");
	if(a == 2)
	    ggprint16(&r, 16, 0x00ff0000, "2");
	if(a == 1)
	    ggprint16(&r, 16, 0x00ff0000, "1");
	if(a == 0)
	    ggprint16(&r, 16, 0x00ff0000, "0");

    }
    //Shape *s2;
    for (int i = 0; i < 1; i++) {
	glColor3ub(90,140,90);
	s = &game->box1[1];
	glPushMatrix();
	s->width = 100;
	s->height = 35;
	s->center.x = WINDOW_WIDTH - 400;
	s->center.y = WINDOW_HEIGHT - 550;
	s->center.z = 0;
	glTranslatef(s->center.x, s->center.y, s->center.z);

	w = s->width;
	h = s->height;

	glBegin(GL_QUADS);
	glVertex2f(-w, -h);
	glVertex2f(-w,  h);
	glVertex2f( w,  h);
	glVertex2f( w, -h);
	glEnd();

	r.bot = WINDOW_HEIGHT - 550;
	r.left = WINDOW_WIDTH - 475;
	glPopMatrix();
	ggprint16(&r, 16, 0x00ff0000, "HIT: ");
	//ggprint16(&r, 0x0033ADFF, "HIT: ");

	for(int i = 0; i < 1; i++) {
	    glColor3ub(90,140,90);
	    s = &game->box1[2];
	    glPushMatrix();
	    s->width = 75;
	    s->height = 35;
	    s->center.x = WINDOW_WIDTH - 150;
	    s->center.y = WINDOW_HEIGHT - 550;
	    s->center.z = 0;
	    glTranslatef(s->center.x, s->center.y, s->center.z);

	    w = s->width;
	    h = s->height;

	    glBegin(GL_QUADS);
	    glVertex2f(-w, -h);
	    glVertex2f(-w,  h);
	    glVertex2f( w,  h);
	    glVertex2f( w, -h);
	    glEnd();

	    r.bot = WINDOW_HEIGHT - 550;
	    r.left = WINDOW_WIDTH - 200;
	    glPopMatrix();
	    ggprint16(&r, 16, 0x00ff0000, "SCORE: ");

	    /*      r.bot = WINDOW_HEIGHT - 575;
		    r.left = WINDOW_WIDTH - 205;

		    ggprint16(&r, 16, 0x00ff0000, " 0 ");
		    */
	}

	//glPushMatrix();
	Duck *d = game->duck;
	glColor3ub(255, 255, 255);
	while(d)
	{
	    w = d->s.width;
	    h = d->s.height;
	    x = d->s.center.x;
	    y = d->s.center.y;
	    glBegin(GL_QUADS);
	    glVertex2f(x-w, y+h);
	    glVertex2f(x-w, y-h);
	    glVertex2f(x+w, y-h);
	    glVertex2f(x+w, y+h);
	    glEnd();
	    d = d->next;
	}
    }
}

    void deleteDuck(Game *game, Duck *node)
    {
	if(node->prev == NULL)
	{
	    if(node->next == NULL)
	    {
		game->duck = NULL;
	    }
	    else
	    {
		node->next->prev = NULL;
		game->duck = node->next;
	    }
	}
	else
	{
	    if(node->next == NULL)
	    {
		node->prev->next = NULL;
	    }
	    else
	    {
		node->prev->next = node->next;
		node->next->prev = node->prev;
	    }
	}
	delete node;
	node = NULL;
	game->n--;
    }
