#define STB_IMAGE_IMPLEMENTATION
#define SDL_MAIN_HANDLED
#include <iostream>
#include <stb_image.h>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <complex>
#include <vector>
#include <windows.h>

using namespace glm;
using namespace std;
using cd = complex<float>;
using cvec3 = vec<3, cd,glm::packed_highp>;
const float PI = 3.14159265359;
// 0 - aplica fft dupa fft-1 dupa afiseaza
// 1 - arata imaginea dupa fft 
#define VISUAL 0

// 0 - dezactiveaza selectarea bandei de frecventa
// 1 - selecteaza doar banda de frecventa [DELIMG_ST,DELIMG_DR]
// 2 - selecteaza totul in afara de banda de frecventa [DELIMG_ST,DELIMG_DR]
#define SELECT_DELIMG 0
#define DELIMG_ST 32
#define DELIMG_DR 256

// 0     - dezactivat
// x > 0 - da overide la imaginea initial cu una sinsoidala
int SIN_IMAGE_FREQ = 0;
int SIN_IMAGE_FREQ2 = 30;

// 1 - sinus perpendicular 
// 2 - sinus pe diagonala
// 3 - sinus compus pe coloana
#define MOD_SIN 3
#define SHIFT_TO_MIDDLE 0
#define ENABLE_SPECTRAL_LEAKAGE 0
int reverse(int num, int lg_n) {
	int res = 0;
	for (int i = 0; i < lg_n; i++) {
		if (num & (1 << i))
			res |= 1 << (lg_n - 1 - i);
	}
	return res;
}


void fft(vector<cd>& a, bool invert) {
	int n = a.size();
	int lg_n = 0;
	while ((1 << lg_n) < n)
		lg_n++;

	for (int i = 0; i < n; i++) {
		if (i < reverse(i, lg_n))
			swap(a[i], a[reverse(i, lg_n)]);
	}

	for (int len = 2; len <= n; len <<= 1) {
		double ang = 2 * PI / len * (invert ? -1 : 1);
		cd wlen(cos(ang), sin(ang));
		for (int i = 0; i < n; i += len) {
#if SHIFT_TO_MIDDLE
			cd w(pow(wlen, a.size() / 2));
#else
			cd w(1);
#endif
			for (int j = 0; j < len / 2; j++) {
				cd u = a[i + j], v = a[i + j + len / 2] * w;
				a[i + j] = u + v;
				a[i + j + len / 2] = u - v;
				w *= wlen;
			}
		}
	}

	if (invert) {
		for (cd& x : a)
			x /= n;
	}
}

int nextPow2(int x) {
	int ans = 1;
	while (ans < x) ans <<= 1;
	return ans;
}

vec3 real(cvec3 x) {
	return { x.x.real(),x.y.real(),x.z.real() };
}

vector<vector<cvec3>> processImage(u8vec3* img, ivec2 size) {
	if(SIN_IMAGE_FREQ)
		for(int i=0;i<size.y;i++)
			for (int j = 0; j < size.x; j++) {
#if MOD_SIN == 2 
				vec2 v(i, j);
				float t = dot(vec2(1, 1), v);
				float f = sin(t *1./SIN_IMAGE_FREQ)*.5 + .5;
#elif MOD_SIN == 3 
				float f = sin(i *1./SIN_IMAGE_FREQ)*.5 + .5;
				float f2 = sin(i * 1. / SIN_IMAGE_FREQ2) * .5 + .5;
				f = .5 * (f2 + f);
#else
				float f = sin(i *1./SIN_IMAGE_FREQ)*.5 + .5;
				float f2 = sin(j * 1. / SIN_IMAGE_FREQ2) * .5 + .5;
				f = .5 * (f2 + f);
#endif
				img[i * size.x + j] = u8vec3(vec3(f, f, f)*255.f);
			}

	int X = nextPow2(size.x);
	int Y = nextPow2(size.y);
	vector<cd> r(X), g(X), b(X);
	vector<cd> r1(Y), g1(Y), b1(Y);
	vector<vector<cvec3>> trans(Y,vector<cvec3>(X,cvec3(0,0,0)));
	for (int i = 0; i < size.x; i++)
		for (int j = 0; j < size.y; j++)
			trans[i][j] = cvec3(vec3(img[i * size.x + j]) / 255.f);
	for (int i = 0; i < Y; i++) { // pentru fiecare rand
		for (int j = 0; j < X; j++) {
			r[j] = trans[i][j].r;
			g[j] = trans[i][j].g;
			b[j] = trans[i][j].b;
		}

		fft(r,0);
		fft(g, 0);
		fft(b, 0);

		for (int j = 0; j < X; j++) {
			trans[i][j] = cvec3(r[j],g[j],b[j]);
		}
	}

	for (int j = 0; j < X; j++) { // pentru fiecare coloana
		for (int i = 0; i < Y; i++) {
			r1[i] = trans[i][j].r;
			g1[i] = trans[i][j].g;
			b1[i] = trans[i][j].b;
		}

		fft(r1, 0);
		fft(g1, 0);
		fft(b1, 0);

		for (int i = 0; i < Y; i++) {
			trans[i][j] = cvec3(r1[i],g1[i],b1[i]);
		}
	}
#if SELECT_DELIMG == 1
	for (int i = 0; i < Y; i++)
		for (int j = 0; j < X; j++)
			if (((j>=DELIMG_ST && j<=X-DELIMG_ST-1) && 
				(i >= DELIMG_ST && i <= DELIMG_DR ||
				i >= Y-DELIMG_DR-1 && i <= Y-DELIMG_ST-1)||
				(i>= DELIMG_ST && i<=Y-DELIMG_ST-1) && 
				(j >= DELIMG_ST && j <= DELIMG_DR ||
				j >= X - DELIMG_DR - 1 && j <= X - DELIMG_ST - 1
				)) == false)
				trans[i][j] = cvec3(0, 0, 0);
#elif SELECT_DELIMG == 2
	for (int i = 0; i < Y; i++)
		for (int j = 0; j < X; j++)
			if ((j >= DELIMG_ST && j <= X - DELIMG_ST - 1) &&
				(i >= DELIMG_ST && i <= DELIMG_DR ||
					i >= Y - DELIMG_DR - 1 && i <= Y - DELIMG_ST - 1) ||
				(i >= DELIMG_ST && i <= Y - DELIMG_ST - 1) &&
				(j >= DELIMG_ST && j <= DELIMG_DR ||
					j >= X - DELIMG_DR - 1 && j <= X - DELIMG_ST - 1
					))
				trans[i][j] = cvec3(0, 0, 0);
#endif 
#if VISUAL
	for (int i = 0; i < size.y; i++)
		for (int j = 0; j < size.x; j++) {
			vec3 val = clamp(log2(vec3(abs(trans[i][j].r), abs(trans[i][j].g), abs(trans[i][j].b)))/10.f, vec3(0, 0, 0), vec3(1, 1, 1));
			img[i * size.x + j] = u8vec3(val * 255.f);
		}
#endif // VISUAL

	return trans;
}


void parseImage(u8vec3* img, ivec2 size, vector<vector<cvec3>>& trans) {
	int Y = trans.size();
	int X = trans[0].size();
	vector<cd> r(X), g(X), b(X);
	vector<cd> r1(Y), g1(Y), b1(Y);


	for (int j = 0; j < X; j++) { // pentru fiecare coloana
		for (int i = 0; i < Y; i++) {
			r1[i] = trans[i][j].r;
			g1[i] = trans[i][j].g;
			b1[i] = trans[i][j].b;
		}

		fft(r1, 1);
		fft(g1, 1);
		fft(b1, 1);

		for (int i = 0; i < Y; i++) {
			trans[i][j] = cvec3(r1[i], g1[i], b1[i]);
		}
	}

	for (int i = 0; i < Y; i++) { // pentru fiecare rand
		for (int j = 0; j < X; j++) {
			r[j] = trans[i][j].r;
			g[j] = trans[i][j].g;
			b[j] = trans[i][j].b;
		}

		fft(r, 1);
		fft(g, 1);
		fft(b, 1);

		for (int j = 0; j < X; j++) {
			trans[i][j] = cvec3(r[j], g[j], b[j]);
		}
	}

	for (int i = 0; i < size.y; i++)
		for (int j = 0; j < size.x; j++) {
			cvec3 x = trans[i][j];
			vec3 xv = vec3(x.r.real(), x.g.real(), x.b.real());
#if ENABLE_SPECTRAL_LEAKAGE == 0
			xv = clamp(xv, vec3(0, 0, 0), vec3(1, 1, 1));
#endif
			img[i * size.x + j] = u8vec3(xv * 255.f);
		}
}

int main()
{
	ShowWindow(GetConsoleWindow(), SW_HIDE);

	ivec2 size;
	int chan;
	u8* img = stbi_load("cat.jpg", &size.x, &size.y, &chan, 3);
	assert(chan == 3);
	auto res = processImage((u8vec3*)img,size);
#if VISUAL == 0
	parseImage((u8vec3*)img, size, res);
#endif
#pragma region DISPLAY LOGIC

	SDL_Rect srcrect = { 0, 0, size.x, size.y };
	SDL_Rect dstrect = { 0, 0, size.x, size.y }; // resized imag size
	
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("Image", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, dstrect.w, dstrect.h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |SDL_WINDOW_BORDERLESS);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(img, size.x, size.y, chan* 8, size.x * chan, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

	SDL_Surface* scaledImage = SDL_CreateRGBSurface(0, dstrect.w, dstrect.h, 32, 0, 0, 0, 0);
	SDL_BlitScaled(surface, &srcrect, scaledImage, &dstrect);
	SDL_BlitSurface(scaledImage, NULL, SDL_GetWindowSurface(window), NULL);
	SDL_UpdateWindowSurface(window);
	// Wait for the window to close
	SDL_Event event;
	while (1) {
		if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
			break;
	}
	// Clean up
	SDL_FreeSurface(scaledImage);
	SDL_DestroyWindow(window);
	SDL_Quit();
	stbi_image_free(img);
	return 0;
#pragma endregion
}