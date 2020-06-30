#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
//#include "TXT.h"

#define MAX_LINE_LENGTH 120
#define MAX_LINES 30

#define BOOL u32
#define TRUE 1
#define FALSE 0

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 576

typedef Uint32 u32;
typedef Uint64 u64;
typedef Sint32 i32;
typedef Sint64 i64;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
}rect_t;

void FillRect(rect_t rect, u32 pixel_color, u32 *screen_pixels)
{
    //assert(screen_pixels);
    for (int row = 0; row < rect.h; ++row)
    {
        for (int col = 0; col < rect.w; ++col)
        {
            screen_pixels[(row + rect.y) * SCREEN_WIDTH + col + rect.x] = pixel_color;
        }
    }
}

int main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Text Editor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    assert(window);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_SOFTWARE);
    assert(renderer);

    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGB888);
    //SDL_assert(format);

    SDL_Texture* screen = SDL_CreateTexture(renderer, format->format, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    assert(screen);

    u32* screen_pixels = (u32*)calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(u32));
    assert(screen_pixels);

    u32 white = SDL_MapRGB(format, 255, 255, 255);
    u32 orange = SDL_MapRGB(format, 255, 165, 0);

    BOOL done = FALSE;

    rect_t txt_box = { 0,0,SCREEN_WIDTH,SCREEN_HEIGHT };
    rect_t caret = { 0, 4, 2, 10};
    u32 glyph_w = TXT_TextWidth("0");
    u32 glyph_h = TXT_TextHeight("0");
    SDL_assert(txt_box.w % glyph_w == 0);
    SDL_assert(txt_box.h % glyph_h == 0);
    u32 caret_color = orange;
    FillRect(caret, 255, screen_pixels);

    BOOL pressed_up = FALSE;
    BOOL pressed_down = FALSE;
    BOOL pressed_right = FALSE;
    BOOL pressed_left = FALSE;

    char curr_ch = '\0';
    char lines[MAX_LINES][MAX_LINE_LENGTH] = { 0 }; 
    u32 last_space_idx[MAX_LINES];
    memset(last_space_idx, -1, sizeof(u32) * MAX_LINES);
    u32 line_idx = 0;
    u32 char_idx = 0;
    u32 start_time = SDL_GetTicks();
    BOOL no_caret = TRUE;

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                done = TRUE;
                break;
            }
            if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
            {
                break;
            }

            SDL_Keycode code = event.key.keysym.sym;

            switch (code)
            {
                case SDLK_ESCAPE:
                    done = event.type == SDL_KEYDOWN;
                    break;
                case SDLK_UP:
                    pressed_up = event.type == SDL_KEYDOWN;
                    break;
                case SDLK_DOWN:
                    pressed_down = event.type == SDL_KEYDOWN;
                    break;
                case SDLK_LEFT:
                    pressed_left = event.type == SDL_KEYDOWN;
                    break;
                case SDLK_RIGHT:
                    pressed_right = event.type == SDL_KEYDOWN;
                    break;
                default:
                    if (event.type != SDL_KEYUP)
                    {
                        curr_ch = (char)code;
                    }
                    break;
            }

            memset(screen_pixels, 0, SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(u32));

            if ((curr_ch >= 'a' && curr_ch <= 'z') || (curr_ch >='0' && curr_ch<= '9') || curr_ch=='.' || curr_ch == ',')
            {
                lines[line_idx][char_idx++] = toupper(curr_ch);
            }
            else if (curr_ch == ' ')
            {
                last_space_idx[line_idx] = char_idx;
                lines[line_idx][char_idx++] = '\0';

            }
            else if (curr_ch == '\b' && char_idx)
            {
                lines[line_idx][--char_idx] = '\0';
            }
            else if (curr_ch == '\n' || curr_ch == '\r')
            {
                ++line_idx;
                char_idx = 0;
            }

            //CHeck if we can collapse two lines into one again
            {
                if (line_idx)
                {
                    i32 previous_line_w = strlen(lines[line_idx - 1]) * glyph_w;
                    i32 curr_line_w = strlen(lines[line_idx]) * glyph_w;

                    if (previous_line_w + curr_line_w < txt_box.w)
                    {
                        char* word = lines[line_idx];
                        memcpy(lines[line_idx - 1] + last_space_idx[line_idx - 1] + 1, lines[line_idx], strlen(lines[line_idx]));
                        memset(lines[line_idx--], 0, MAX_LINE_LENGTH);
                        char_idx = txt_box.w / glyph_w - 1;
                    }
                }
            }

            // Move word to next line if line is too full
            {
                i32 line_w = (char_idx+1) * glyph_w;
                if (line_w > txt_box.w)
                {
                    if (last_space_idx[line_idx] != -1)
                    {
                        char* overlapping_word = lines[line_idx] + last_space_idx[line_idx] + 1;
                        i32 word_size = char_idx - last_space_idx[line_idx];
                        memcpy(lines[++line_idx], overlapping_word, word_size * sizeof(char));
                        memset(lines[line_idx - 1] + line_idx + 1, 0, word_size * sizeof(char));
                        char_idx = word_size - 1;
                    }
                    else
                    {
                        lines[line_idx][0] = lines[line_idx][char_idx - 1];
                        lines[line_idx][char_idx - 1] = '\0';
                        ++line_idx;
                        char_idx = 1;
                    }
                }
            }

            //Updating the screen's pixels based on the new text buffer
            {
                i32 y = 0;
                for (i32 i = 0; i <= line_idx; i++)
                {
                    y = i * (glyph_h + 5);
                    TXT_DrawText(lines[i], 0, y, screen_pixels);
                }
            }

            
            curr_ch = '\0';

            TXT_DrawText(lines, 0, 0, screen_pixels);

            caret.x = strlen(lines[line_idx]) * glyph_w + 1;

            FillRect(caret, no_caret ? 0 : white, screen_pixels);

            /*char* msg = "A blue square that can breathe...";
            int width = TXT_TextWidth(msg);*/

            //TXT_DrawText(msg, (SCREEN_WIDTH - width) / 2, 10, screen_pixels);

            /*FillRect(square, 255, screen_pixels);*/

            if (SDL_GetTicks() - start_time > 500)
            {
                no_caret = !no_caret;
                start_time = SDL_GetTicks();
            }

            SDL_UpdateTexture(screen, NULL, screen_pixels, SCREEN_WIDTH * sizeof(u32));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, screen, NULL, NULL);
            SDL_RenderPresent(renderer);

            SDL_Delay(50);
        }
    }

    return 0;
}

