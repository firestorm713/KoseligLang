#include"SDL_GUI.h"

float linearInterp(float y1, float y2, float mu)
{
	return (y1*(1 - mu) + y2*mu);
}

float cosInterp(float y1, float y2, float mu)
{
	float mu2;

	mu2 = (1 - (float)cos(mu*M_PI)) / 2;
	return(y1*(1 - mu2) + y2*mu2);
}

float cubicInterp(float y0, float y1, float y2, float y3, float mu)
{
	float a0, a1, a2, a3, mu2;
	mu2 = mu*mu;
	a0 = y3 - y2 - y0 + y1;
	a1 = y0 - y1 - a0;
	a2 = y2 - y0;
	a3 = y1;

	return (a0*mu*mu2 + a1*mu2 + a2*mu + a3);
}

float hermiteInterp(float y0, float y1, float y2, float y3, float mu, float tension, float bias)
{
	float m0, m1, mu2, mu3;
	float a0, a1, a2, a3;

	mu2 = mu*mu;
	mu3 = mu2*mu;

	m0 =  (y1 - y0)*(1 + bias)*(1 - tension) / 2;
	m0 += (y2 - y1)*(1 - bias)*(1 - tension) / 2;
	m1 =  (y2 - y1)*(1 + bias)*(1 - tension) / 2;
	m1 += (y3 - y2)*(1 - bias)*(1 - tension) / 2;
	a0 = 2 *  mu3 - 3 * mu2 + 1;
	a1 =      mu3 - 2 * mu2 + mu;
	a2 =      mu3 -     mu2;
	a3 = -2 * mu3 + 3 * mu2;

	return(a0*y1 + a1*m0 + a2*m1 + a3*y2);
}

void PutPixel32_nolock(SDL_Surface* surface, int x, int y, Uint32 color)
{
	Uint8* pixel = (Uint8*)surface->pixels;
	pixel += (y * surface->pitch) + (x* sizeof(Uint32));
	*((Uint32*)pixel) = color;
}

void PutPixel24_nolock(SDL_Surface* surface, int x, int y, Uint32 color)
{
	Uint8* pixel = (Uint8*)surface->pixels;
	pixel += (y * surface->pitch) + (x*sizeof(Uint8) * 3);
#if SDL_BYTEORDER==SDL_BIG_ENDIAN
	pixel[0] = (color >> 24) & 0xFF;
	pixel[1] = (color >> 16) & 0xFF;
	pixel[2] = (color >> 8) & 0xFF;
#else
	pixel[0] = color & 0xFF;
	pixel[1] = (color >> 8) & 0xFF;
	pixel[2] = (color >> 16) & 0xFF;
#endif
}

void PutPixel16_nolock(SDL_Surface* surface, int x, int y, Uint32 color)
{
	Uint8* pixel = (Uint8*)surface->pixels;
	pixel += (y * surface->pitch) + (x * sizeof(Uint16));
	*((Uint16*)pixel) = color & 0xFFFF;
}

void PutPixel8_nolock(SDL_Surface* surface, int x, int y, Uint32 color)
{
	Uint8* pixel = (Uint8*)surface->pixels;
	pixel += (y * surface->pitch) + (x * sizeof(Uint8));
	*pixel = color & 0xFF;
}

void PutPixel32(SDL_Surface* surface, int x, int y, Uint32 color)
{
	if (SDL_MUSTLOCK(surface))
		SDL_LockSurface(surface);
	PutPixel32_nolock(surface, x, y, color);
	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

void PutPixel24(SDL_Surface* surface, int x, int y, Uint32 color)
{
	if (SDL_MUSTLOCK(surface))
		SDL_LockSurface(surface);
	PutPixel24_nolock(surface, x, y, color);
	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

void PutPixel16(SDL_Surface* surface, int x, int y, Uint32 color)
{
	if (SDL_MUSTLOCK(surface))
		SDL_LockSurface(surface);
	PutPixel16_nolock(surface, x, y, color);
	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

void PutPixel8(SDL_Surface* surface, int x, int y, Uint32 color)
{
	if (SDL_MUSTLOCK(surface))
		SDL_LockSurface(surface);
	PutPixel8_nolock(surface, x, y, color);
	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

void PutPixel(SDL_Surface* surface, int x, int y, Uint32 color)
{
	SDL_PixelFormat* fmt = surface->format;
	if (fmt->BitsPerPixel == 32)
		PutPixel32(surface, x, y, color);
	else if (fmt->BitsPerPixel == 24)
		PutPixel24(surface, x, y, color);
	else if (fmt->BitsPerPixel == 16)
		PutPixel16(surface, x, y, color);
	else //(fmt->BitsPerPixel == 8)
		PutPixel8(surface, x, y, color);
}

Uint32 GetPixel(SDL_Surface* surface, int x, int y)
{
	if (SDL_MUSTLOCK(surface))
		SDL_LockSurface(surface);
	Uint8* pixel = (Uint8*)surface->pixels;
	pixel += (y * surface->pitch) + (x* sizeof(Uint32));
	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
	return *((Uint32*)pixel);
}

SDL_Surface* CreateSurfaceFrom(int width, int height, SDL_Surface* display)
{
	const SDL_PixelFormat* fmt = display->format;
	return SDL_CreateRGBSurface(0, width, height, fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
}

Uint32 BlendColors(Uint32 top, Uint32 bottom, SDL_PixelFormat* fmt)
{
	SDL_Color topColor, bottomColor, finalColor;
	SDL_GetRGBA(top, fmt, &topColor.r, &topColor.g, &topColor.b, &topColor.a);
	SDL_GetRGBA(bottom, fmt, &bottomColor.r, &bottomColor.g, &bottomColor.b, &bottomColor.a);
	
	if (bottomColor.r > 127.5f)
	{
		float value = (float)(255 - bottomColor.r) / 127.5f;
		float min = bottomColor.r - (255 - bottomColor.r);
		float overlay = (float)topColor.r*value + min;
		finalColor.r = overlay;
	}
	else{
		float value = (float)bottomColor.r / 127.5f;
		float overlay = (float)topColor.r*value;
		finalColor.r = overlay;
	}
	if (bottomColor.g > 127.5f)
	{
		float value = (float)(255 - bottomColor.g) / 127.5f;
		float min = bottomColor.g - (255 - bottomColor.g);
		float overlay = (float)topColor.g*value + min;
		finalColor.g = overlay;
	}
	else{
		float value = (float)bottomColor.g / 127.5f;
		float overlay = (float)topColor.g*value;
		finalColor.g = overlay;
	}
	if (bottomColor.b > 127.5f)
	{
		float value = (float)(255 - bottomColor.b) / 127.5f;
		float min = bottomColor.b - (255 - bottomColor.b);
		float overlay = (float)topColor.b*value + min;
		finalColor.b = overlay;
	}
	else{
		float value = (float)bottomColor.b / 127.5f;
		float overlay = (float)topColor.b*value;
		finalColor.b = overlay;
	}

	if (bottomColor.a > 127.5f)
	{
		float value = (float)(255 - bottomColor.a) / 127.5f;
		float min = bottomColor.a - (255 - bottomColor.a);
		float overlay = (float)topColor.a*value + min;
		finalColor.a = overlay;
	}
	else{
		float value = (float)bottomColor.a / 127.5f;
		float overlay = (float)topColor.a*value;
		finalColor.a = overlay;
	}
	
	return SDL_MapRGBA(fmt, finalColor.r, finalColor.g, finalColor.b, finalColor.a);
}

