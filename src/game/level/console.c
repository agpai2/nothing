#include <assert.h>

#include "console.h"
#include "game/edit_field_ring.h"
#include "game/edit_field.h"
#include "game/level.h"
#include "game/level/player/rigid_rect.h"
#include "script/gc.h"
#include "script/interpreter.h"
#include "script/parser.h"
#include "script/scope.h"
#include "system/error.h"
#include "system/lt.h"
#include "sdl/renderer.h"

#define FONT_WIDTH_SCALE 3.0f
#define FONT_HEIGHT_SCALE 3.0f
#define SLIDE_DOWN_TIME 0.1f
#define SLIDE_DOWN_SPEED (FONT_CHAR_HEIGHT * FONT_HEIGHT_SCALE / SLIDE_DOWN_TIME)
#define CONSOLE_HEIGHT (FONT_HEIGHT_SCALE * FONT_CHAR_HEIGHT)

struct Console
{
    Lt *lt;
    Gc *gc;
    struct Scope scope;
    EditFieldRing *edit_field_ring;
    Level *level;
    float y;
};

/* TODO(#354): Console does not allow to travel the history by pressing up and down */
/* TODO(#355): Console does not support Emacs keybindings */
/* TODO(#356): Console does not support autocompletion */
/* TODO(#357): Console does not show the state of the GC of the script */
/* TODO(#358): Console does not support copy, cut, paste operations */

static struct EvalResult rect_apply_force(void *param, Gc *gc, struct Scope *scope, struct Expr args)
{
    assert(gc);
    assert(scope);
    assert(param);

    Level *level = (Level*) param;
    const char *rect_id = CAR(args).atom->str;
    struct Expr vector_force_expr = CAR(CDR(args));
    const float force_x = (float) CAR(vector_force_expr).atom->num;
    const float force_y = (float) CDR(vector_force_expr).atom->num;

    print_expr_as_sexpr(args); printf("\n");

    Rigid_rect *rigid_rect = level_rigid_rect(level, rect_id);
    if (rigid_rect != NULL) {
        printf("Found rect `%s`\n", rect_id);
        printf("Applying force (%f, %f)\n", force_x, force_y);
        rigid_rect_apply_force(rigid_rect, vec(force_x, force_y));
    } else {
        fprintf(stderr, "Couldn't find rigid_rect `%s`", rect_id);
    }

    return eval_success(NIL(gc));
}

Console *create_console(Level *level,
                        const Sprite_font *font)
{
    Lt *lt = create_lt();

    if (lt == NULL) {
        return NULL;
    }

    Console *console = PUSH_LT(lt, malloc(sizeof(Console)), free);
    if (console == NULL) {
        throw_error(ERROR_TYPE_LIBC);
        RETURN_LT(lt, NULL);
    }
    console->lt = lt;

    console->gc = PUSH_LT(lt, create_gc(), destroy_gc);
    if (console->gc == NULL) {
        RETURN_LT(lt, NULL);
    }

    console->scope.expr = CONS(console->gc,
                               NIL(console->gc),
                               NIL(console->gc));
    set_scope_value(
        console->gc,
        &console->scope,
        SYMBOL(console->gc, "rect-apply-force"),
        NATIVE(console->gc, rect_apply_force, level));

    console->edit_field_ring = PUSH_LT(
        lt,
        create_edit_field_ring(
            font,
            vec(FONT_WIDTH_SCALE, FONT_HEIGHT_SCALE),
            color(0.80f, 0.80f, 0.80f, 1.0f),
            10),
        destroy_edit_field_ring);
    if (console->edit_field_ring == NULL) {
        RETURN_LT(lt, NULL);
    }

    console->level = level;
    console->y = -CONSOLE_HEIGHT;

    return console;
}

void destroy_console(Console *console)
{
    assert(console);
    RETURN_LT0(console->lt);
}

int console_handle_event(Console *console,
                         const SDL_Event *event)
{
    switch(event->type) {
    case SDL_KEYDOWN:
        switch(event->key.keysym.sym) {
        case SDLK_RETURN: {
            const char *source_code = edit_field_as_text(edit_field_ring_get(console->edit_field_ring, 0));
            struct ParseResult parse_result = read_expr_from_string(console->gc,
                                                                    source_code);
            if (parse_result.is_error) {
                /* TODO(#359): Console doesn't report any parsing errors visually */
                print_parse_error(stderr, source_code, parse_result);
                return 0;
            }

            struct EvalResult eval_result = eval(
                console->gc,
                &console->scope,
                parse_result.expr);

            if (eval_result.is_error) {
                /* TODO(#360): Console doesn't report any eval errors visually */
                printf("Error:\t");
                print_expr_as_sexpr(eval_result.expr);
                printf("\n");
            }

            gc_collect(console->gc, console->scope.expr);

            edit_field_clean(edit_field_ring_get(console->edit_field_ring, 0));

        } return 0;
        }
        break;
    }

    return edit_field_handle_event(edit_field_ring_get(console->edit_field_ring, 0),
                                   event);
}

int console_render(const Console *console,
                   SDL_Renderer *renderer)
{
    /* TODO(#364): console doesn't have any padding around the edit fields */
    SDL_Rect view_port;
    SDL_RenderGetViewport(renderer, &view_port);

    if (fill_rect(renderer,
                  rect(0.0f, console->y,
                       (float) view_port.w,
                       CONSOLE_HEIGHT),
                  color(0.20f, 0.20f, 0.20f, 1.0f)) < 0) {
        return -1;
    }

    /* TODO: Console does not render the full Edit field ring */
    if (edit_field_render(edit_field_ring_get(console->edit_field_ring, 0),
                          renderer,
                          vec(0.0f, console->y)) < 0) {
        return -1;
    }

    return 0;
}

int console_update(Console *console, float delta_time)
{
    assert(console);

    /* TODO(#366): console slide down animation doesn't have any easing */

    if (console->y < 0.0f) {
        console->y += SLIDE_DOWN_SPEED * delta_time;

        if (console->y > 0.0f) {
            console->y = 0.0f;
        }
    }

    return 0;
}

void console_slide_down(Console *console)
{
    assert(console);
    console->y = -CONSOLE_HEIGHT;
}
