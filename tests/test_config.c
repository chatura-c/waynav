/*
 * Tests for config parser.
 */

#include "../src/waynav.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct xkb_keymap *keymap = NULL;

static void write_tmp_config(const char *content, const char *path) {
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(content, f);
    fclose(f);
}

static void build_keymap(void) {
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    assert(context);
    struct xkb_rule_names names = {
        .layout = "us",
    };
    keymap = xkb_keymap_new_from_names(context, &names,
                                       XKB_KEYMAP_COMPILE_NO_FLAGS);
    assert(keymap);
    xkb_context_unref(context);
}

static const struct binding *find_binding_by_keysym(const struct config *cfg,
                                                     xkb_keysym_t sym,
                                                     uint32_t mods) {
    xkb_keycode_t keycode = config_keycode_for_keysym(keymap, sym);
    return config_find_binding(cfg, keycode, mods);
}

static void test_basic_parse(void) {
    const char *path = "/tmp/waynav_test_config";
    write_tmp_config("clear\n"
                     "super+semicolon start,grid 4x4\n"
                     "h move-left,warp\n"
                     "shift+h cut-left,warp\n"
                     "space warp,click 1\n"
                     "semicolon end\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);
    assert(cfg.line_width == GRID_LINE_WIDTH_DEFAULT);

    /* start binding is stored separately. */
    assert(cfg.num_start_commands == 1);
    assert(cfg.start_commands[0].type == CMD_GRID);
    assert(cfg.start_commands[0].arg.grid.cols == 4);
    assert(cfg.start_commands[0].arg.grid.rows == 4);

    /* 4 normal bindings: h, shift+h, space, semicolon */
    assert(cfg.num_bindings == 4);

    /* h -> move-left, warp */
    const struct binding *b = find_binding_by_keysym(&cfg, XKB_KEY_h, 0);
    assert(b);
    assert(b->num_commands == 2);
    assert(b->commands[0].type == CMD_MOVE_LEFT);
    assert(b->commands[1].type == CMD_WARP);

    /* shift+h -> cut-left, warp */
    b = find_binding_by_keysym(&cfg, XKB_KEY_h, MOD_SHIFT);
    assert(b);
    assert(b->num_commands == 2);
    assert(b->commands[0].type == CMD_CUT_LEFT);
    assert(b->commands[1].type == CMD_WARP);

    /* space -> warp, click 1 */
    b = find_binding_by_keysym(&cfg, XKB_KEY_space, 0);
    assert(b);
    assert(b->num_commands == 2);
    assert(b->commands[0].type == CMD_WARP);
    assert(b->commands[1].type == CMD_CLICK);
    assert(b->commands[1].arg.button == 1);

    /* semicolon -> end */
    b = find_binding_by_keysym(&cfg, XKB_KEY_semicolon, 0);
    assert(b);
    assert(b->num_commands == 1);
    assert(b->commands[0].type == CMD_END);

    unlink(path);
}

static void test_line_width(void) {
    const char *path = "/tmp/waynav_test_config_line_width";
    write_tmp_config("clear\n"
                     "line-width 3.5\n"
                     "semicolon end\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    assert(cfg.line_width == 3.5);
    assert(cfg.num_bindings == 1);

    unlink(path);
}

static void test_invalid_line_width_keeps_previous_value(void) {
    const char *path = "/tmp/waynav_test_config_invalid_line_width";
    write_tmp_config("line-width 2.5\n"
                     "line-width 0\n"
                     "line-width -1\n"
                     "line-width nan\n"
                     "line-width inf\n"
                     "line-width 1e309\n"
                     "line-width 3px\n"
                     "line-width2\n"
                     "semicolon end\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    assert(cfg.line_width == 2.5);
    assert(cfg.num_bindings == 1);

    unlink(path);
}

static void test_cell_select(void) {
    const char *path = "/tmp/waynav_test_config2";
    write_tmp_config("clear\n"
                     "1 cell-select 1,warp\n"
                     "v cell-select 16,warp\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);
    assert(cfg.num_bindings == 2);

    const struct binding *b = find_binding_by_keysym(&cfg, XKB_KEY_1, 0);
    assert(b);
    assert(b->commands[0].type == CMD_CELL_SELECT);
    assert(b->commands[0].arg.cell == 1);

    b = find_binding_by_keysym(&cfg, XKB_KEY_v, 0);
    assert(b);
    assert(b->commands[0].type == CMD_CELL_SELECT);
    assert(b->commands[0].arg.cell == 16);

    unlink(path);
}

static void test_shell_command(void) {
    const char *path = "/tmp/waynav_test_config3";
    write_tmp_config("clear\n"
                     "grave shell 'notify deprecated'\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);
    assert(cfg.num_bindings == 1);

    const struct binding *b = find_binding_by_keysym(&cfg, XKB_KEY_grave, 0);
    assert(b);
    assert(b->commands[0].type == CMD_SHELL);
    assert(strcmp(b->commands[0].arg.shell_cmd, "notify deprecated") == 0);

    free(b->commands[0].arg.shell_cmd);
    unlink(path);
}

static void test_cursorzoom(void) {
    const char *path = "/tmp/waynav_test_config4";
    write_tmp_config("clear\n"
                     "i grid 1x1,cursorzoom 10 10\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);
    assert(cfg.num_bindings == 1);

    const struct binding *b = find_binding_by_keysym(&cfg, XKB_KEY_i, 0);
    assert(b);
    assert(b->num_commands == 2);
    assert(b->commands[0].type == CMD_GRID);
    assert(b->commands[0].arg.grid.cols == 1);
    assert(b->commands[0].arg.grid.rows == 1);
    assert(b->commands[1].type == CMD_CURSORZOOM);
    assert(b->commands[1].arg.zoom.w == 10);
    assert(b->commands[1].arg.zoom.h == 10);

    unlink(path);
}

static void test_drag(void) {
    const char *path = "/tmp/waynav_test_config5";
    write_tmp_config("clear\n"
                     "shift+space warp,drag 1\n"
                     "shift+minus warp,drag 3\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);
    assert(cfg.num_bindings == 2);

    const struct binding *b =
        find_binding_by_keysym(&cfg, XKB_KEY_space, MOD_SHIFT);
    assert(b);
    assert(b->commands[1].type == CMD_DRAG);
    assert(b->commands[1].arg.button == 1);

    b = find_binding_by_keysym(&cfg, XKB_KEY_minus, MOD_SHIFT);
    assert(b);
    assert(b->commands[1].type == CMD_DRAG);
    assert(b->commands[1].arg.button == 3);

    unlink(path);
}

static void test_rejects_command_prefix_extension(void) {
    /* A command whose name merely extends a real keyword (clicker,
     * dragon) must not be mis-parsed as that keyword. The chain then
     * yields no commands, so the binding is dropped. */
    const char *path = "/tmp/waynav_test_config_prefix";
    write_tmp_config("clear\n"
                     "x clicker 1\n"
                     "y dragon 2\n"
                     "z click 1\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);

    /* Only the well-formed "click" binding survives. */
    assert(cfg.num_bindings == 1);
    const struct binding *b = find_binding_by_keysym(&cfg, XKB_KEY_z, 0);
    assert(b);
    assert(b->commands[0].type == CMD_CLICK);
    assert(b->commands[0].arg.button == 1);

    unlink(path);
}

static void test_colors(void) {
    const char *path = "/tmp/waynav_test_config_colors";
    write_tmp_config("grid-color ff0000\n"
                     "region-bg 11223344\n"
                     "grid-color abc        # shorthand re-sets grid_color\n"
                     "line-width 2.5\n"
                     "grid-color nope       # malformed: leaves prior value\n",
                     path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);

    /* ff0000 is overwritten by valid shorthand abc -> aabbccff;
     * the malformed value after it is rejected and changes nothing. */
    assert(cfg.grid_color == 0xaabbccff);
    assert(cfg.region_bg == 0x11223344);
    assert(cfg.line_width == 2.5);

    unlink(path);
}

static void test_color_defaults(void) {
    const char *path = "/tmp/waynav_test_config_defaults";
    write_tmp_config("clear\n", path);

    struct config cfg;
    assert(config_load(&cfg, path) == 0);
    assert(cfg.grid_color == GRID_COLOR_DEFAULT);
    assert(cfg.region_bg == REGION_BG_DEFAULT);
    assert(cfg.line_width == GRID_LINE_WIDTH_DEFAULT);

    unlink(path);
}

static void test_shifted_bindings_match_the_physical_key(void) {
    const char *path = "/tmp/waynav_test_config_shifted_bindings";
    write_tmp_config("h cut-left,warp\n"
                     "shift+h move-left,warp\n"
                     "shift+1 click 1\n"
                     "shift+asterisk drag 2\n",
                     path);

    struct config cfg = {0};
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);

    xkb_keycode_t keycode_h = config_keycode_for_keysym(keymap, XKB_KEY_h);
    xkb_keycode_t keycode_1 = config_keycode_for_keysym(keymap, XKB_KEY_1);
    xkb_keycode_t keycode_shift =
        config_keycode_for_keysym(keymap, XKB_KEY_Shift_L);
    struct xkb_state *state = xkb_state_new(keymap);
    assert(keycode_h != XKB_KEYCODE_INVALID);
    assert(keycode_1 != XKB_KEYCODE_INVALID);
    assert(keycode_shift != XKB_KEYCODE_INVALID);
    assert(state);
    xkb_state_update_key(state, keycode_shift, XKB_KEY_DOWN);
    assert(xkb_state_key_get_one_sym(state, keycode_h) == XKB_KEY_H);
    assert(xkb_state_key_get_one_sym(state, keycode_1) == XKB_KEY_exclam);

    const struct binding *binding =
        find_binding_by_keysym(&cfg, XKB_KEY_h, MOD_SHIFT);
    assert(binding);
    assert(binding->commands[0].type == CMD_MOVE_LEFT);

    binding = find_binding_by_keysym(&cfg, XKB_KEY_h, 0);
    assert(binding);
    assert(binding->commands[0].type == CMD_CUT_LEFT);

    binding = find_binding_by_keysym(&cfg, XKB_KEY_1, MOD_SHIFT);
    assert(binding);
    assert(binding->commands[0].type == CMD_CLICK);
    assert(binding->commands[0].arg.button == 1);

    binding = find_binding_by_keysym(&cfg, XKB_KEY_8, MOD_SHIFT);
    assert(binding);
    assert(binding->commands[0].type == CMD_DRAG);
    assert(binding->commands[0].arg.button == 2);

    xkb_state_unref(state);
    unlink(path);
}

static void test_later_binding_overrides_earlier(void) {
    const char *path = "/tmp/waynav_test_config_binding_override";
    write_tmp_config("shift+period click 1\n"
                     "shift+greater click 3\n",
                     path);

    struct config cfg = {0};
    assert(config_load(&cfg, path) == 0);
    config_resolve_keycodes(&cfg, keymap);
    assert(cfg.num_bindings == 2);

    const struct binding *binding =
        find_binding_by_keysym(&cfg, XKB_KEY_period, MOD_SHIFT);
    assert(binding);
    assert(binding->commands[0].type == CMD_CLICK);
    assert(binding->commands[0].arg.button == 3);

    unlink(path);
}

int main(void) {
    build_keymap();

    test_basic_parse();
    test_line_width();
    test_invalid_line_width_keeps_previous_value();
    test_cell_select();
    test_shell_command();
    test_cursorzoom();
    test_drag();
    test_rejects_command_prefix_extension();
    test_colors();
    test_color_defaults();
    test_shifted_bindings_match_the_physical_key();
    test_later_binding_overrides_earlier();

    xkb_keymap_unref(keymap);
    printf("All config tests passed.\n");
    return 0;
}
