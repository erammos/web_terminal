#include "SDL_events.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <wchar.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/fetch.h>
#endif

// Screen dimensions
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
#define HISTORY_MAX 10000
#define INPUT_MAX 100
#define FOLDER_MAX 10
#define FONT_SIZE 20
#define MAX_LINES 10000

// SDL variables
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
TTF_Font *font = nullptr;
char history_buffer[HISTORY_MAX] = {};
char active_buffer[INPUT_MAX + FOLDER_MAX] = {};
char input[INPUT_MAX] = {};

size_t input_len = 0;
size_t history_len = 0;
size_t cursor_pos = 0;
uint32_t time_ms = 0;
bool show_cursor = false;
uint32_t startTime = 0;
char current_cursor[FOLDER_MAX] = "#";
float aspect_ratio = SCREEN_WIDTH / (float)SCREEN_HEIGHT;
size_t current_line = 0;
size_t total_lines = 0;

#define LS "Documents  about.txt  linkedin.sh"
#define PUSH(x) push_text(x, strlen(x))
const char logo[] = {
#embed "../assets/banner.txt"
	, '\0'
};

const char about[] = {
#embed "../assets/about.txt"
	, '\0'
};
char *track_lines[MAX_LINES] = { 0 };

void log_file_download(const char *file_name)
{
	// Directly use Firebase Analytics inside EM_ASM
#ifdef __EMSCRIPTEN__
	EM_ASM(
		{
			var fileName = UTF8ToString($0);
			Module.logDownloadEventFromC(fileName);
		},
		file_name);
#endif
}

void open_url(const char *url)
{
#ifdef __EMSCRIPTEN__
	EM_ASM(
		{
			var url = UTF8ToString($0);
			window.open(url, "_self");
		},
		url);
#endif
}

void download_file(const char *url)
{
#ifdef __EMSCRIPTEN__
	EM_ASM(
		{
			var url = UTF8ToString($0);
			// Create a temporary link and trigger the download
			var link = document.createElement('a');
			link.href = url;
			link.download = ''; // Optional: leave blank to use the default filename from the server
			document.body.appendChild(link);
			link.click();
			document.body.removeChild(link);
		},
		url);
	log_file_download("cv");
#endif
}
char *get_current_line()
{
	return track_lines[current_line];
}

SDL_Rect draw_text(const char buffer[], int x, int y)
{
	SDL_Color foreground = (SDL_Color){ .r = 0, .g = 255, .b = 0 };
	SDL_Surface *surface =
		TTF_RenderText_Solid_Wrapped(font, buffer, foreground, 1920);

	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_Rect dest = {};
	dest.x = x;
	dest.y = y;
	dest.w = surface->w;
	dest.h = surface->h;
	SDL_RenderCopy(renderer, texture, NULL, &dest);
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
	return dest;
}

SDL_Rect draw_history(int x, int y)
{
	SDL_Rect res = { .x = x, .y = y };
	if (history_len == 0)
		return res;
	res = draw_text(get_current_line(), x, y);
	return res;
}
void scroll_line(int scroll_y)
{
	if (history_len > 0) {
		if (scroll_y < 0 && current_line < total_lines - 1) {
			current_line++;
		} else if (scroll_y > 0 && current_line > 0) {
			current_line--;
		}
	}
}

void animate_cursor(uint32_t duration)
{
	input_len = strlen(input);
	auto current_cursor_len = strlen(current_cursor);
	memcpy(active_buffer, current_cursor, current_cursor_len);
	memcpy(active_buffer + current_cursor_len, input, input_len);
	cursor_pos = input_len + current_cursor_len;

	if ((SDL_GetTicks() - startTime) >= duration) {
		startTime = SDL_GetTicks();
		show_cursor = !show_cursor;
	}

	active_buffer[cursor_pos] = (show_cursor) ? '_' : ' ';
	active_buffer[cursor_pos + 1] = '\0';
}

void clear_history()
{
	current_line = 0;
	total_lines = 0;
	history_buffer[0] = '\0';
	history_len = 0;
}

void auto_scroll()
{
	if (total_lines - current_line > 35) {
		current_line += (total_lines - current_line) - 35;
	}
}

void push_text(const char text[], size_t text_len)
{
	//CHECK OVERFLOWS
	if (history_len + text_len >= HISTORY_MAX) {
		clear_history();
		return;
	}
	if (history_len > 0) {
		history_buffer[history_len++] = '\n';
	}
	char *latest_line = history_buffer + history_len;
	memcpy(latest_line, text, text_len);
	if (latest_line[text_len - 1] == '\n') {
		text_len--;
	}
	history_len += text_len;
	history_buffer[history_len] = '\0';

	char *t = strchr(latest_line, '\n');
	while (t != nullptr) {
		track_lines[total_lines++] = latest_line;
		latest_line = t + 1;
		t = strchr(latest_line, '\n');
	}
	if (latest_line[0] != '\0')
		track_lines[total_lines++] = latest_line;

	auto_scroll();
}

void input_reset_cursor()
{
	input[0] = '\0';
	input_len = 0;
}

void resize_callback(int new_width, int new_height)
{
	float new_aspect_ratio = (float)new_width / (float)new_height;

	if (new_aspect_ratio > aspect_ratio) {
		new_width = (int)(new_height * aspect_ratio);
	} else {
		new_height = (int)(new_width / aspect_ratio);
	}

	SDL_SetWindowSize(window, new_width, new_height);
	SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH,
				 SCREEN_HEIGHT); // Keeps rendering consistent
}
// Main loop function for Emscripten
void main_loop()
{
	bool running = true;
	SDL_Event event;

	// Handle events
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			running = false;
			break;
		case SDL_TEXTINPUT:
			if (input_len + strlen(event.text.text) < INPUT_MAX) {
				strcat(input, event.text.text);
			}
			break;
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_BACKSPACE) {
				if (input_len > 0)
					input[--input_len] = '\0';

			} else if (event.key.keysym.sym == SDLK_RETURN) {
				push_text(active_buffer, cursor_pos);
				if (strcmp(input, "ls") == 0) {
					PUSH(LS);
				} else if (strcmp(input, "clear") == 0) {
					clear_history();
				} else if (strcmp(input, "cat about.txt") ==
					   0) {
					PUSH(about);
				} else if (strcmp(input, "./linkedin.sh") == 0) {
					open_url("https://www.linkedin.com/in/elias-rammos-b3548739");
				} else if (input_len > 0) {
					PUSH("Unknown command :-P");
				}
				input_reset_cursor();
			}
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				resize_callback(event.window.data1,
						event.window.data2);
			}
			break;
		case SDL_MOUSEWHEEL:
			scroll_line(event.wheel.y);
			break;
		}
	}

	if (!running) {
#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
#else
		exit(0);
#endif
		SDL_StopTextInput();
	}

	animate_cursor(1000);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);
	int yoffset = 50;
	draw_text(logo, SCREEN_WIDTH / 2 - 310, yoffset);
	SDL_Rect rect = draw_history(0, yoffset + 150);
	draw_text(active_buffer, 0, rect.h + rect.y);
	;
	// Present the screen
	SDL_RenderPresent(renderer);
}

int main(void)
{
	/* Initialize the TTF library */
	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		return 1;
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	if (TTF_Init() < 0) {
		SDL_Log("Couldn't initialize TTF: %s\n", TTF_GetError());
		SDL_Quit();
		return 1;
	}
	double ptsize = FONT_SIZE;
	font = TTF_OpenFont("./assets/font.ttf", ptsize);
	if (font == nullptr) {
		SDL_Log("Couldn't load %g pt font from %s: %s\n", ptsize,
			"font.ttf", SDL_GetError());
		exit(1);
	}
	// Create SDL window
	window = SDL_CreateWindow("terminal", SDL_WINDOWPOS_UNDEFINED,
				  SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
				  SCREEN_HEIGHT,
				  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Quit();
		return 1;
	}
	// Create SDL renderer
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	track_lines[0] = &history_buffer[0];

	SDL_StartTextInput();
#ifdef __EMSCRIPTEN__
	// Set the main loop to run with Emscripten
	emscripten_set_main_loop(main_loop, 0, 1);
#else
	// Native platform event loop

	// Render the red screen
	while (1) {
		main_loop();
	}

	// Delay to cap frame rate (optional, can be removed for unlimited FPS)

	// Cleanup SDL
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
#endif

	return 0;
}
