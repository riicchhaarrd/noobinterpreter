#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int sf_createwindow(ipr_t *ipr) {
	return 0;
}

static int sf_mousex(ipr_t *ipr) {

	HWND con = GetConsoleWindow();
	RECT rect;

	GetWindowRect(con, &rect);


	POINT p;
	GetCursorPos(&p);

	se_addint(ipr, p.x - rect.left);

	return 1;
}

static int sf_leftmousepressed(ipr_t *ipr) {
	se_addbool(ipr, GetAsyncKeyState(VK_LBUTTON)==VK_LBUTTON);
	return 1;
}

static int sf_rightmousepressed(ipr_t *ipr) {
	se_addbool(ipr, GetAsyncKeyState(VK_RBUTTON)==VK_RBUTTON);
	return 1;
}

static int sf_mousey(ipr_t *ipr) {

	HWND con = GetConsoleWindow();
	RECT rect;

	GetWindowRect(con, &rect);


	POINT p;
	GetCursorPos(&p);

	se_addint(ipr, p.y - rect.top);

	return 1;
}

static int sf_set_pixel(ipr_t *ipr) {

	int x = se_getint(ipr, 0);
	int y = se_getint(ipr, 1);

	int r = se_getint(ipr, 2);
	int g = se_getint(ipr, 3);
	int b = se_getint(ipr, 4);

	HWND console_hwnd = GetConsoleWindow();
	HDC console_hdc = GetDC(console_hwnd);

	SetPixelV(console_hdc, x, y, RGB(r, g, b));

	ReleaseDC(console_hwnd, console_hdc);

	return 0;
}

stockfunction_t scriptfunctions_graphics[] = {
	{ "leftmousepressed",sf_leftmousepressed },
	{"rightmousepressed",sf_rightmousepressed},
	{ "mousex",sf_mousex },
	{"mousey",sf_mousey},
	{ "create_window", sf_createwindow },
	{ "set_pixel", sf_set_pixel },
	{ NULL, 0 }
};