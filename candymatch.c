/*
 * Author: Kyle Kloberdanz
 * Project Start Date: 13 July 2019
 * License: GNU GPLv3 (see LICENSE.txt)
 *     This file is part of candymatch.
 *
 *     candymatch is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     candymatch is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with candymatch.  If not, see <https://www.gnu.org/licenses/>.
 * File: candymatch.c
 */

#include <stdbool.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define MAX(A, B) (((A) > (B)) ? (A) : (B))

enum {
    SCREEN_WIDTH = 800,
    SCREEN_HEIGHT = 600,
    MAX_VELOCITY = 20,
    MIN_VELOCITY = 10,
    WINNING_SCORE = 10
};

enum Direction {
    UP = 1,
    DOWN = 2,
    LEFT = 4,
    RIGHT = 8
};

enum Button {
    B = 0,
    A = 1,
    START = 9,
    SELECT = 8
};

struct Entity {
    SDL_Rect rect;
    SDL_Texture *texture;
};

struct Character {
    struct Entity entity;
    int x_vel;
    int y_vel;
};

int rand_ball_velocity() {
    int random_num = rand() % MAX_VELOCITY;
    int velocity = MAX(random_num, MIN_VELOCITY);
    return velocity;
}

bool obj_touching(SDL_Rect *rect1, SDL_Rect *rect2) {
    return !(rect1->x >= rect2->x + rect2->w) &&
           !(rect1->y >= rect2->y + rect2->h) &&
           !(rect2->x >= rect1->x + rect1->w) &&
           !(rect2->y >= rect1->y + rect1->h);
}

bool obj_in_bounds(SDL_Rect *rect) {
    if (rect->x > SCREEN_WIDTH) {
        return false;
    } else if (rect->x < 0) {
        return false;
    } else if (rect->y > SCREEN_HEIGHT) {
        return false;
    } else if (rect->y < 0) {
        return false;
    } else {
        return true;
    }
}

char *get_button_str(enum Button button) {
    switch (button) {
        case B:
            return "B";
        case A:
            return "A";
        case START:
            return "START";
        case SELECT:
            return "SELECT";
        default:
            return "NOT A BUTTON";
    }
}

int get_button(SDL_Joystick *joystick) {
    int num_buttons = SDL_JoystickNumButtons(joystick);
    for (int i = 0; i < num_buttons; i++) {
        if (SDL_JoystickGetButton(joystick, i)) {
            return i;
        }
    }
    return -1;
}

char get_direction(SDL_Joystick *joystick) {
    int num_axes = SDL_JoystickNumAxes(joystick);
    char direction = 0;
    for (int i = 0; i < num_axes; i++) {
        int axis = SDL_JoystickGetAxis(joystick, i);
        if (axis) {
            if (i == 1) {
                if (axis < 0) {
                    direction |= UP;
                } else {
                    direction |= DOWN;
                }
            } else if (i == 0) {
                if (axis < 0) {
                    direction |= LEFT;
                } else {
                    direction |= RIGHT;
                }
            }
        }
    }
    return direction;
}

SDL_Surface *load_image(char *filename, SDL_Surface *screen) {
    SDL_Surface *surface = IMG_Load(filename);
    SDL_Surface *optimized_surface = NULL;

    if (surface == NULL) {
        fprintf(stderr, "unable to load image: %s\n", filename);
    } else {
        int color_key = SDL_MapRGB(surface->format, 0x00, 0x00, 0x00);
        SDL_SetColorKey(surface, SDL_TRUE, color_key);
        optimized_surface = SDL_ConvertSurface(surface, screen->format, 0);
        SDL_FreeSurface(surface);
    }
    return optimized_surface;
}

SDL_Texture *load_texture(
    char *filename,
    SDL_Surface *screen,
    SDL_Renderer *renderer
) {
    SDL_Surface *image = load_image(filename, screen);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
    SDL_FreeSurface(image);
    return texture;
}

bool game_running() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;

            default:
                break;
        }
    }
    return true;
}

int main(void) {
    /* initialize */
    SDL_Init(SDL_INIT_EVERYTHING);
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        fprintf(stderr, "failed to initialize image library\n");
        exit(EXIT_FAILURE);
    };
    SDL_Renderer *renderer;
    SDL_Window *window;
    int num_joysticks = SDL_NumJoysticks();
    printf("found %d joysticks\n", num_joysticks);
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            printf("Joystick %i is supported!\n", i);
        }
    }

    SDL_Joystick *joystick = NULL;
    if (num_joysticks) {
        joystick = SDL_JoystickOpen(0);
    } else {
        exit(EXIT_FAILURE);
    }

    /* create window and renderer */
    SDL_CreateWindowAndRenderer(
        0,
        0,
        SDL_WINDOW_FULLSCREEN,
        &window,
        &renderer
    );

    if (SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT) < 0) {
        fprintf(stderr, "failed to set resolution: %s\n", SDL_GetError());
    }

    SDL_Surface *screen_surface = SDL_GetWindowSurface(window);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_Texture *background_image = load_texture("assets/background.png", screen_surface, renderer);

    /* populate cake textures */
    SDL_Texture *cake_textures[13];
    char file_str[255];
    for (int i = 0; i < 13; i++) {
        sprintf(file_str, "assets/cake_eaten%d.png", i);
        cake_textures[i] = load_texture(file_str, screen_surface, renderer);
    }

    struct Entity cake = {
        .rect = {
            .x = 0,
            .y = 0,
            .w = 22 * 4,
            .h = 22 * 4
        },
        .texture = cake_textures[0]
    };

    int x_vel = 10;
    int y_vel = 10;
    //int grow_rate = 2;
    int cake_frame = 1;
    while (game_running()) {
        int start_tick = SDL_GetTicks();
        char dir = get_direction(joystick);
        if (dir) {
            if (dir & DOWN) {
                if (cake.rect.y < SCREEN_HEIGHT - cake.rect.h) {
                    cake.rect.y += y_vel;
                }
            }
            if (dir & UP) {
                if (cake.rect.y > 0) {
                    cake.rect.y -= y_vel;
                }
            }
            if (dir & RIGHT) {
                if (cake.rect.x < SCREEN_WIDTH - cake.rect.w) {
                    cake.rect.x += x_vel;
                }
            }
            if (dir & LEFT) {
                if (cake.rect.x > 0) {
                    cake.rect.x -= x_vel;
                }
            }
        }

        char button = get_button(joystick);
        switch (button) {
            case A:
                /*
                if (cake.rect.w < SCREEN_WIDTH) {
                    cake.rect.w += grow_rate;
                    cake.rect.h += grow_rate;
                }
                */
                cake.texture = cake_textures[cake_frame++ % 13];
                break;
            case B:
                /*
                if (cake.rect.w > 0) {
                    cake.rect.w -= grow_rate;
                    cake.rect.h -= grow_rate;
                }
                */
                break;
            case START:
                goto exit_gameloop;
            default:
                break;
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, background_image, NULL, NULL);
        SDL_RenderCopy(renderer, cake.texture, NULL, &cake.rect);
        SDL_RenderPresent(renderer);

        SDL_Delay(20 - (start_tick - SDL_GetTicks()));
    }
exit_gameloop:

    puts("destroy window");
    SDL_DestroyWindow(window);
    puts("shutting down sdl");
    SDL_Quit();
    puts("done");
    return 0;
}
