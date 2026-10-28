// A capture starts while the game's confirm input is held. Its release must reach input.c, or confirm is still
// held when the capture ends and the game's Options screen immediately starts another capture (#175).
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>

#include "input_bind.h"

static uint64_t now;
Uint64 SDL_GetTicks(void) { return now; }
int input_pad_family(void) { return 1; }

// Model the held state on the other side of the same event routing as the window loop. No window or controller.
static bool held;
static void release_to_game(SDL_Event e) {
    assert(!bind_capture_event(&e));
    held = false;
}

static void waiting(void) {
    int dev, slot, ms;
    assert(bind_capture_active(&dev, &slot, &ms));
}

static void ended(int expected) {
    int dev, slot, other, serial, ms;
    assert(!bind_capture_active(&dev, &slot, &ms));
    assert(bind_last_result(&dev, &slot, &other, &serial) == expected);
    assert(!held);
}

int main(void) {
    SDL_Event e = {0};
    held = true;
    bind_capture_begin(BIND_DEV_KEY, BIND_RIGHT);
    e.type = SDL_EVENT_KEY_UP; e.key.scancode = SDL_SCANCODE_E;
    release_to_game(e); waiting();  // releasing confirm must not assign a binding
    e.type = SDL_EVENT_KEY_DOWN; e.key.scancode = SDL_SCANCODE_B;
    assert(bind_capture_event(&e));
    assert(g_bind_key[BIND_RIGHT] == SDL_SCANCODE_B);
    ended(1);

    held = true;
    bind_capture_begin(BIND_DEV_KEY, BIND_RIGHT);
    e = (SDL_Event){0}; e.type = SDL_EVENT_MOUSE_BUTTON_UP; e.button.button = SDL_BUTTON_LEFT;
    release_to_game(e); waiting();
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.button = SDL_BUTTON_X1;
    assert(bind_capture_event(&e));
    assert(g_bind_key[BIND_RIGHT] == BIND_MOUSE + SDL_BUTTON_X1);
    ended(1);

    held = true;
    bind_capture_begin(BIND_DEV_PAD, BIND_RIGHT);
    e = (SDL_Event){0}; e.type = SDL_EVENT_GAMEPAD_BUTTON_UP; e.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
    release_to_game(e); waiting();
    e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; e.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    assert(bind_capture_event(&e)); ended(2);  // B already uses the south button: swap

    const int devices[] = {BIND_DEV_KEY, BIND_DEV_PAD};
    for (unsigned i = 0; i < sizeof devices / sizeof *devices; i++) {
        int dev = devices[i];
        for (int axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER; axis <= SDL_GAMEPAD_AXIS_RIGHT_TRIGGER; axis++) {
            held = true;
            bind_capture_begin(dev, BIND_A);
            e = (SDL_Event){0}; e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            e.gaxis.axis = axis; e.gaxis.value = 0;
            release_to_game(e); waiting();
            e = (SDL_Event){0}; e.type = SDL_EVENT_KEY_DOWN; e.key.scancode = SDL_SCANCODE_ESCAPE;
            assert(bind_capture_event(&e)); ended(3);
        }
    }

    held = true;
    bind_capture_begin(BIND_DEV_KEY, BIND_A);
    e = (SDL_Event){0}; e.type = SDL_EVENT_KEY_UP; e.key.scancode = SDL_SCANCODE_E;
    release_to_game(e); now += BIND_CAPTURE_MS;
    ended(4);  // idle timeout also leaves confirm released

    // A new binding still needs a press, and sticks must not navigate the remapping UI during capture.
    bind_capture_begin(BIND_DEV_PAD, BIND_A);
    e = (SDL_Event){0}; e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX; e.gaxis.value = 20000;
    assert(bind_capture_event(&e)); waiting();
    e.gaxis.axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    assert(bind_capture_event(&e));
    assert(g_bind_pad[BIND_A] == PADB_LTRIGGER);
    ended(2);  // ZL already uses the left trigger
    // Applying a preset must handle existing custom bindings without duplicating an input, and persist on reload.
    bind_xbox_face_preset();
    assert(g_bind_pad[BIND_A] == SDL_GAMEPAD_BUTTON_SOUTH);
    assert(g_bind_pad[BIND_B] == SDL_GAMEPAD_BUTTON_EAST);
    assert(g_bind_pad[BIND_X] == SDL_GAMEPAD_BUTTON_NORTH);
    assert(g_bind_pad[BIND_Y] == SDL_GAMEPAD_BUTTON_WEST);
    for (int i = 0; i < BIND_PAD_COUNT; i++)
        for (int j = i + 1; j < BIND_PAD_COUNT; j++) assert(g_bind_pad[i] != g_bind_pad[j]);
    FILE *f = tmpfile();
    assert(f);
    bind_settings_save(f);
    rewind(f);
    bind_reset(BIND_DEV_PAD);
    char line[256], key[128], value[128];
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%127s = %127s", key, value) == 2) bind_settings_load(key, value);
    fclose(f);
    assert(g_bind_pad[BIND_A] == SDL_GAMEPAD_BUTTON_SOUTH);
    assert(g_bind_pad[BIND_B] == SDL_GAMEPAD_BUTTON_EAST);
    assert(g_bind_pad[BIND_X] == SDL_GAMEPAD_BUTTON_NORTH);
    assert(g_bind_pad[BIND_Y] == SDL_GAMEPAD_BUTTON_WEST);
    puts("binding capture and Xbox preset passed");
    return 0;
}
