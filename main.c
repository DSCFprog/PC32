#include <GLFW/glfw3.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>
#include <mikmod.h>


/*		SPECS
	32bit compiler
	Simplified launch
	auto frees malloc when COLD called
*/

// Sound
MODULE* module;
SAMPLE* samples[4];
int	  voices[4];
int	  modplaying = 1;

// threading
pthread_t THREADS[4];


// OGL
GLFWwindow* window;
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;

uint IWIDTH = 640;
uint IHEIGHT = 480;
float SCALE = 4096.0f;
#define ISCALE (1.0f/SCALE)

// MACROS ----
// #define STDC __attribute__((stdcall))
#define SRCSZE (1440*2880)>>2		
#define BLKNME "data.blk"
#define WW extern uint*
#define XX uint* STK

// MEM ----
extern uint* DATABLK;
extern uint* CDATA;		// 0-9 INPUT 10...29 C func 30... GL func
extern ushort WIDTH;
extern ushort HEIGHT;

uint MALLOC_ARRAY[512];
uint* CMALLOC = &MALLOC_ARRAY;
void freemalloc() {
	for(int i = 0; i < 512; i++)
		if(MALLOC_ARRAY[i]) { free(MALLOC_ARRAY[i]); MALLOC_ARRAY[i] = 0; }	
	CMALLOC = &MALLOC_ARRAY;
}

uint* blkread() {
	FILE* fs = fopen(BLKNME, "rb");
	if(!fs) return NULL;
	uint* mem = (uint*)&DATABLK;
	fread((char*)mem, 4, SRCSZE, fs);
	fclose(fs);
	printf("-- memory @: %X\n", mem);
	return DATABLK;
}

void blkwrite() {
	FILE* fs = fopen(BLKNME, "wb"); fseek(fs, 0, SEEK_SET);
	fwrite(DATABLK, 4, SRCSZE, fs);
	fflush(fs); fclose(fs);
}

// fps
char* fps_msg[32];
double fps_elapsed = 0, fps_lastime = 0;
uint fps_framecnt = 0;
void fps() {
   double ctime = glfwGetTime();
   double res = ctime-fps_lastime;
   fps_elapsed += ctime - fps_lastime;
   fps_lastime = ctime;
   ++fps_framecnt;
   if(fps_elapsed >= 1.0)  {
      sprintf((char*)fps_msg,"PC32 FPS: %d", fps_framecnt);
      glfwSetWindowTitle(window, (const char*)fps_msg);
      fps_framecnt = 0; fps_elapsed = 0;
   }
}

// CALLBACKS --------------------------------------------------------------

void resize(GLFWwindow* window, int W, int H) { 
	WIDTH = W; HEIGHT = H; (&CDATA)[0] = 1; 
	//glPixelZoom((float)W/1280.0f, (float)H/1024.0f);
	glPixelZoom((float)W/(float)IWIDTH, (float)H/(float)IHEIGHT);
	glViewport(0, 0, IWIDTH, IHEIGHT);
}

void key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
   if(!action || key < 200 ) return;
   (&CDATA)[2] = key;
   (&CDATA)[3] = mods;
   //printf("key: %d scan: %d action: %d mods: %d \n", key, scancode, action, mods);
}

void char_cb(GLFWwindow* window, uint unicode) {
   (&CDATA)[1] = unicode;
   //printf("char: %d\n", unicode);
}

void mousepos_cb(GLFWwindow* window, double x, double y) {
	int mx = ((int)(((float)x/(float)WIDTH)*((float)IWIDTH)));
	int my = (int)(((float)y/(float)HEIGHT)*((float)IHEIGHT));
   (&CDATA)[4] = ((ushort)my)<<16 | (ushort)mx; }

void mousebot_cb(GLFWwindow* window, int button, int action, int mods) {
   (&CDATA)[5] = ((button+1)<<4)|action&0xF; }

void mousescr_cb(GLFWwindow* window, double x, double y)
{  (&CDATA)[6] = (int32_t)y; }

void cjoy(uint X) {
   int acnt = 0, bcnt = 0, hcnt = 0;
   float* axes = glfwGetJoystickAxes(X, &acnt);
   char* butn = glfwGetJoystickButtons(X, &bcnt);
   char* hats = glfwGetJoystickHats(X, &hcnt);
	(&CDATA)[7] = acnt;
	(&CDATA)[8] = bcnt;
	(&CDATA)[9] = hcnt;
}


// C FNS
extern uint cprint(char* msg) {
	for(int i = 0; i < 16; i++)
		printf("%c", msg[i]);
	printf("?\n");
	return -1;
}

uint cdebug(XX) { 
	printf("called from C! \n");
	return 0xAABB;
}

char* cdot(char* X) {
	int len = X[-1]&15;
	printf("P:");
	for(int i = 0; i < len; i++)
		printf("%c", X[i]);
	printf("\n");
	return X;
}

char* cpystr(char* from, char* to) {
	int len = from[-1]&15;
	for(int i = 0; i < len; i++)
		to[i] = from[i];
	to[len] = 0;
	return to;
}

void cblk_write(XX) { blkwrite(); return STK; }
void csleep(uint X) { if(X) usleep(X); }
uint copen(char* path, char* mode) {
	char p[256]; char m[256];
	cpystr(path, p); cpystr(mode, m);
	FILE* han = fopen(p, m);
	if(!han) { 
		printf("OPEN FAILED! | CREATING FILE: %s\n", p);
		han = fopen(p, "w+");
	}
	return han;
	return fopen(p, m);
}
void cclose(uint handle) { fclose(handle); }
uint cflush(uint handle) { fflush(handle); return handle; }
uint cwrite(uint from, uint to, uint count) { 
	int sze = count>>30 ? 4 : 1;
	fwrite(from, sze, count&0x0FFFFFFF, to); return to;	
}
uint cread(uint from, uint to, uint count)	{ 
	int sze = count>>30 ? 4 : 1;
	fread(to, sze, count&0x0FFFFFFF, from); return to;		
}
uint ctell(uint handle) { return ftell(handle); }
uint cseek(uint pos, uint handle) { fseek(handle, pos, SEEK_SET); return handle; }
uint csize(uint handle) { 	
	fseek(handle, 0, SEEK_END); 
	int sze = ftell(handle); 
	fseek(handle, 0, SEEK_SET);
	return sze; 
}
uint ctimeu(void) {
	struct timeval te; 
	gettimeofday(&te, NULL);
	return te.tv_usec; // te.tv_sec*1000LL + te.tv_usec/1000; 
}


void csystem(XX) { }

// GL FNS
void glbg(uint color) {
	float r = ((float)(color>>16))/255.0f;
	float g = ((float)((color>>8)&0xFF))/255.0f;
	float b = ((float)(color&0xFF))/255.0f;
	glClearColor(r, g, b, 1.0f);
}

uint glwindow() { return window; }

void TEST(uint X, uint Y, uint type, uint packing, uint* data) {
	
	uint xtype = GL_RGB; 
	uint xpack = GL_UNSIGNED_SHORT_5_6_5;
	glDrawPixels(X, Y, type, packing, data);
	int u = 10;
}


void cresetmatrix() {
	float X = ISCALE;
	//glMatrixMode(GL_PROJECTION);
	//glLoadIdentity();
	//glScalef(X, X, X);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(X, X, X);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(X, X, X);
}

void ctranslate(int z, int y, int x) {glTranslatef((float)x, (float)y, (float)z); } 
void cscale(int z, int y, int x) { glScalef((float)x, (float)y, (float)z); } 
void crotate(int deg, int flag) { glRotatef((float)deg/SCALE, flag&0xFF, flag&0xFF00, flag&0xFF0000); } 
void ctexture(int ifmt, int extfmt, int datatype, int sze, int* data) {
	int sx = sze&0xFFFF;
	int sy = (sze>>16)&0xFFFF;
	int u = (int)GL_RGBA;
	int t = (int)GL_UNSIGNED_SHORT_5_5_5_1;
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, data); 
	glTexImage2D(GL_TEXTURE_2D, 0, ifmt, sx, sy, 0, 
					extfmt, datatype, data); 
}

void cortho(int x, int y) {
	glOrtho(	
				(float)(((short)x>>16)&0xFFFF)/SCALE,
				(float)((short)x&0xFFFF)/SCALE,
				(float)(((short)y>>16)&0xFFFF)/SCALE,
				(float)((short)y&0xFFFF)/SCALE,
				0.01f, 10000000.0f);
}
void cpersp(int fov, int aspect) {
	float f = (float)fov*ISCALE;
	float a = (float)aspect* ISCALE;
	gluPerspective(f, a, 0.01f, 10000000.0f); }

void cuv(int x, int y) {
	glTexCoord2i(x, y);
}

void cvert(int x, int y, int z) {
	glVertex3i(x, y, z);
}

void ccolor(int c) {
	float	a = (float)(c&0xFF)/255.0f;
	c = c>>8;
	float b = (float)(c&0xFF)/255.0f;
	float g = (float)((c>>8)&0xFF)/255.0f;
	float r = (float)((c>>16)&0xFF)/255.0f;
	glColor4f(r, g, b, a);
}
void cnormal(int c) { glNormal3b(c&0xFF, (c>>8)&0xFF, (c>>16)&0xFF); }
void cscpy(int fmt, int type, int pos, int sze, int* data) {
	glReadPixels(pos&0xFFFF, (pos>>16)&0xFFFF, sze&0xFFFF, (sze>>16)&0xFFFF, fmt, type, data);
}

int csqrt(int X) {
	float number = (float)X * ISCALE;
	int i;
	float x2, y;
	const float threehalfs = 1.5F;
	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;                     
	i  = 0x5f3759df - ( i >> 1 );              
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );
	return (int)(y*4096.0f);
}

void lpsze(int x) {
	if(x < 0) glLineWidth(abs(x)*ISCALE);
	glPointSize(x*ISCALE);
}

void gldummy() {	
	
}

void gltest() {

}

void cwinscale(uint x, uint y) { 
	IWIDTH = x; IHEIGHT = y;
	int W; int H;
	glfwGetWindowSize(window, &W, &H);
	glPixelZoom((float)W/(float)IWIDTH, (float)H/(float)IHEIGHT);
	glViewport(0, 0, IWIDTH, IHEIGHT);
}

// MikMod
uint sndmodplayfile(char* path) {
	char p[256];
	cpystr(path, p);
	module = Player_Load(p, 64, 0);
	if(module) { Player_Start(module); return module; }
	fprintf(stderr, "could not load module: %s \n", 
			MikMod_strerror(MikMod_errno));
	return 0;
}

uint sndmodplay(char* addr, uint length) {
	module = Player_LoadMem(addr, length, 64, 0);
	if(module) { modplaying = 1; Player_Start(module); return module; }
	fprintf(stderr, "could not load module: %s \n", 
			MikMod_strerror(MikMod_errno));
	return 0;
}

void sndmodflag(uint val, uint flag) {
	if(!module) return;
	switch(flag) {
		case(0): { module->volume = val&0x7F; break; }			
		case(1): { module->wrap = val&1; break; }
		case(2): { module->loop = val&1; break; }
		case(3): { module->fadeout = val&1; break; }
	}
}

void sndmodstop() { 
	if(!module) return; 
	Player_Stop(); Player_Free(module);
}

int sndupdate() {

}

uint sndloadsample(char* addr, uint length, uint idx) {
	SAMPLE* sample = samples[idx&3];
	if(sample) Sample_Free(sample);
	sample = Sample_LoadMem(addr, length);
	samples[idx&3] = sample;
	if(sample) return sample;
	fprintf(stderr, "could not load sample: %s \n", 
			MikMod_strerror(MikMod_errno));
	return 0;
}

void sndplaysample(uint idx) {
	SAMPLE* sample = samples[idx&3];
	if(sample) {
		int voice = Sample_Play(sample, 0, 0);
		Voice_SetPanning(voice, PAN_CENTER);
		voices[idx] = voice;
	}
}

uint sndsamplestop(uint idx) { Sample_Free(samples[idx&3]); }

void sndsampleflag(uint val, uint prop, uint idx) {
	SAMPLE* sample = samples[idx&3];
	switch(prop) {
		case(0): sample->panning = val; break;
		case(1): sample->speed = val; break;
		case(2): sample->volume = val; break;
	}
}

void sound_thread() {
	printf("sound thread active\n");
	while(!glfwWindowShouldClose(window)) {
		MikMod_Update();
		usleep(10000);
	}
}

void initsound() {
	MikMod_RegisterAllDrivers();
	MikMod_RegisterAllLoaders();
	md_mode |= DMODE_SOFT_MUSIC|DMODE_SOFT_SNDFX;
	if(MikMod_Init("")) {
		fprintf(stderr, "mikmod couldnt start: %s\n", 
				MikMod_strerror(MikMod_errno));
		return;
	}
	MikMod_SetNumVoices(-1, 4);
	MikMod_EnableOutput();
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, sound_thread, NULL);
}

// threading
void* thread_mem[4] = {0, 0, 0, 0};
void thread_wait0() {
	printf("threads waiting \n");
	while(!glfwWindowShouldClose(window)) {
		if(thread_mem[0]) { 
			void* fn = thread_mem[0];	
			thread_mem[0] = 0;
			((void(*)()) fn)();
		}
		usleep(10000);
	}
}

//void thread_store(uint fn, uint idx) { thread_mem[idx&3] = fn; }


// MAIN
extern uint comp(void);
#define INTERN(X) ANS[0] = (uint)X; ANS++
void FORTH() {
	blkread();
	// C intern 30 fns max
	uint* ANS = &(CDATA)+10;
	INTERN(&csleep);
	INTERN(&copen);
	INTERN(&cclose);
	INTERN(&cflush);
	INTERN(&cwrite);
	INTERN(&cread);
	INTERN(&ctell);
	INTERN(&cseek);
	INTERN(&csize);
	INTERN(&ctimeu);
	INTERN(&cjoy);
	
	// GL intern 226 fns max
	ANS = &(CDATA)+30;
	INTERN(&glbg);
	INTERN(&glwindow);
	INTERN(&glfwPollEvents);
	INTERN(&glfwWindowShouldClose);
	INTERN(&glClear);
	INTERN(&glDrawPixels);
	INTERN(&glfwSwapBuffers);
	INTERN(&glEnable);
	INTERN(&glDisable);
	INTERN(&glBlendFunc);
	INTERN(&glBindTexture);

	// GLFW hints & window
	INTERN(&cwinscale);

	// GL 3D
	ANS = &(CDATA)+80;
	INTERN(&glBegin);
	INTERN(&glEnd);
	INTERN(&glGenTextures);
	INTERN(&ctexture);
	INTERN(&glTexParameteri);
	INTERN(&glMatrixMode);
	INTERN(&glPushMatrix);
	INTERN(&glPopMatrix);
	INTERN(&glLoadIdentity);
	INTERN(&cresetmatrix);
	INTERN(&ctranslate);	
	INTERN(&crotate);
	INTERN(&cscale);
	INTERN(&cortho);
	INTERN(&cpersp);
	INTERN(&glVertex3i);		// vert pos
	INTERN(&glTexCoord2i);	// vert uv
	INTERN(&ccolor);			// vert color
	INTERN(&cnormal);			// vert normal
	INTERN(&cscpy);			
	INTERN(&csqrt);
	INTERN(&lpsze);
	INTERN(&gldummy);

	// Sound Modfiles
	ANS = &(CDATA)+180;
	INTERN(&sndmodplay);
	INTERN(&sndmodplayfile);
	INTERN(&sndmodflag);
	INTERN(&sndmodstop);
	INTERN(&sndupdate);
	INTERN(&Player_ToggleMute);
	INTERN(&Player_TogglePause);
	INTERN(&Player_SetVolume);

	// Sound Samples
	INTERN(&sndloadsample);
	INTERN(&sndplaysample);
	INTERN(&sndsamplestop);
	INTERN(&sndsampleflag);


	//glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0f);
	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glCullFace(GL_BACK);
	glPointSize(8.0f);
	glLineWidth(3.0f);
	cresetmatrix();
	//glColor4f(1.0f, 0, 0, 1.0f);

	uint* res = comp();
	printf("%X\n", res[0]);
	printf("%X\n", res[1]);
	printf("%X\n", res[2]);
	printf("OK\n");
}

int main() {
	glfwInit();

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);	
	glfwWindowHint(GLFW_DECORATED, NULL);
	//glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
	window = glfwCreateWindow(IWIDTH, IHEIGHT, "PC32", NULL, NULL);
	//glfwSetWindowSize(window, 1280, 1024);
	glfwMakeContextCurrent(window);
	//glfwSwapInterval(0);
   glfwSetCursorPosCallback(window, mousepos_cb);
   glfwSetMouseButtonCallback(window, mousebot_cb);
   glfwSetScrollCallback(window, mousescr_cb);
   glfwSetKeyCallback(window, key_cb);
   glfwSetCharCallback(window, char_cb);
   glfwSetWindowSizeCallback(window, resize);

	//sound init
	initsound();

	FORTH();
   glfwSetWindowShouldClose(window, 1);
   glfwTerminate();
	MikMod_Exit();
	// pthread_join(thread_id, NULL);
}







