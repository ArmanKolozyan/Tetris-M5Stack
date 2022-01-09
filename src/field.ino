#include "EEPROM.h"
#include <M5StickC.h>
#include <stdio.h>
#include <stdlib.h>
#include "constants.h"
#include "installed_pieces.h"

// Since these two operations come in handy very often, I created the two abstractions below so as not to have to do the same computation all the time.
struct double_node *field_get(struct double_node **values, short row, short col) {
    short index = row * FIELD_WIDTH + col;
    return values[index];
}

void field_set(struct double_node **values, short row, short col, struct double_node *parent) {
    short index = row * FIELD_WIDTH + col;
    values[index] = parent;
}

/**
 * Deletes a field block from the playing field. In case of a timed block, the dynamically allocated
 * memory is feeed.
 */
void delete_component(struct game_state *game_state, struct double_node *component, uint8_t column, uint8_t row) {
    if (component) { // checking if not NULL
        delete_position(&component->value, column, row);
        if (field_get(game_state->field, row, column)->value.color == 9) {
            if (field_get(game_state->field, row, column)->value.blocks_left == 0) {
                game_state->timed_block.active = 0;
            }
        } else if (field_get(game_state->field, row, column)->value.blocks_left == 0) {
            Serial.printf("giuuu");
            game_state->score += LINE_CLEAR_SCORE_INCREASE;
            game_state->active_pieces = delete_node(component->value.id, game_state->active_pieces);
        }
        field_set(game_state->field, row, column, NULL);
    }
}



/**
 * Checks whether the tetromino that is currently falling down can move further.
 * It returns a number that indicates corresponding information.
 * 2 => outside the vertical bounds (width)
 * 3 => outside the horizontal bounds (height)
 * 4 => in the position of a component, there is already another component
 * 5 => the tetromino can move safely
 */
uint8_t piece_fits(struct tetromino_state tetromino_state, struct game_state *game_state, uint8_t width, uint8_t height) {

    struct tetromino *tetromino = game_state->tetromino_state.tetromino;

    for (short row = 0;
         row < tetromino->length;
         ++row) {
        for (short column = 0;
             column < tetromino->length;
             ++column) {
            uint8_t component_value = get_component(tetromino, row, column, tetromino_state.rotation);
            if (component_value > 0) {
                short field_row = tetromino_state.row_in_field + row;
                short field_column = tetromino_state.column_in_field + column;
                if (field_column >= 0 && field_column < FIELD_WIDTH) {
                    if (field_row >= 0 && field_row < FIELD_HEIGHT) {
                        if (field_get(game_state->field, field_row, field_column)) {
                            return COLLISION;
                        }
                    } else {
                        return OUTSIDE_HEIGHT;
                    }
                } else {
                    return OUTSIDE_WIDTH;
                }
            }
        }
    }
    return CAN_MOVE;
}


/**
 * Installs the current tetromino in the field (when it can no longer move).
 * Installation is done by pointing each of its components in the field
 * to a newly added installed_piece_data node.
 */
void install_tetromino_in_field(struct game_state *game) {
    struct installed_piece_data value = create_installed_piece(game->current_id, determine_color(game->tetromino_state.tetromino->blocks, game->tetromino_state.tetromino->length), game->tetromino_state.tetromino->blocks_amount);
    game->active_pieces = insert_before(value, game->active_pieces);
    struct tetromino *tetromino = game->tetromino_state.tetromino;
    for (uint8_t row = 0;
         row < tetromino->length;
         ++row) {
        for (uint8_t column = 0;
             column < tetromino->length;
             ++column) {
            uint8_t value = get_component(tetromino, row, column, game->tetromino_state.rotation);
            if (value) {
                short field_row = game->tetromino_state.row_in_field + row;
                short field_column = game->tetromino_state.column_in_field + column;
                struct double_node *new_node = game->active_pieces;
                uint8_t free_index = game->active_pieces->value.free_index; // in een aparte functie want komt 2 keer voor
                game->active_pieces->value.positions[free_index] = (uint8_t)field_column;
                game->active_pieces->value.positions[free_index + 1] = (uint8_t)field_row;
                game->active_pieces->value.free_index += 2;
                field_set(game->field, field_row, field_column, new_node);
            }
        }
    }
    if (determine_color(game->tetromino_state.tetromino->blocks, game->tetromino_state.tetromino->length) == 9) {
        free(game->tetromino_state.tetromino->blocks);
        free(game->tetromino_state.tetromino);
    }
}



uint8_t possible_to_lower(struct game_state *game_state) {
    tetromino_state tetromino = game_state->tetromino_state;
    tetromino.row_in_field += 1;
    return (piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT) == CAN_MOVE);
}


/**
 * Removes the given row and brings down all the rows above it.
 */
void clear_line(game_state *game_state, short y) {
    for (uint8_t line_x = 0; line_x < FIELD_WIDTH; line_x++) {
        for (short line_y = y; line_y > 0; line_y--) {
            if (line_y == y) {
                struct double_node *current_component = field_get(game_state->field, line_y, line_x);
                delete_component(game_state, current_component, line_x, line_y);
            }
            field_set(game_state->field, line_y, line_x, field_get(game_state->field, line_y - 1, line_x));
            if (field_get(game_state->field, line_y - 1, line_x)) { // check for NULL (i.e. empty) block
                change_position(&field_get(game_state->field, line_y, line_x)->value, line_x, line_y - 1, line_x, line_y);
                field_set(game_state->field, line_y - 1, line_x, NULL);
            }
        }
        game_state->field[line_x] = NULL;
    }
}

/**
 * Given a column and a row, this function brings down all components that
 * lie above the given row in the given column until a hole is encountered.
 * This function is needed to implement the logic of mines.
 */
void bring_down(struct double_node **field, short x, short y) {
    for (short line_y = y; line_y > 0; line_y--) {
        struct double_node *component_above = field_get(field, line_y - 1, x);
        field_set(field, line_y, x, component_above);
        if (!component_above) { // check for NULL (i.e. empty) block
            return;
        } else {
            change_position(&field_get(field, line_y, x)->value, x, line_y - 1, x, line_y);
            field_set(field, line_y - 1, x, NULL);
        }
    }
}


/**
 * This function checks whether the given row is fully occupied by blocks.
 * When it encounters a hole in the row it returns 0 (false), otherwise it returns 1(true).
 */
uint8_t is_line_full(double_node **field, short y) {
    uint8_t line_full = 1;
    for (uint8_t block_x = 1; block_x < FIELD_WIDTH; block_x++) {
        if (!field_get(field, y, block_x)) { // NULL-pointer check
            line_full = 0;
            return line_full;
        }
    }
    return line_full;
}

/**
 * Chechks if the tetromino that just got installed caused a full line formation.
 * If so, the affected lines are cleared.
 */
void handle_full_lines(struct game_state *game_state) {
    for (uint8_t block_y = 0; block_y < game_state->tetromino_state.tetromino->length; block_y++) {
        if (game_state->tetromino_state.row_in_field + block_y < FIELD_HEIGHT) {
            if (is_line_full(game_state->field, game_state->tetromino_state.row_in_field + block_y)) {
                clear_line(game_state, game_state->tetromino_state.row_in_field + block_y);
                game_state->speed.counter--;
                game_state->score += 150;
            }
        }
    }
}