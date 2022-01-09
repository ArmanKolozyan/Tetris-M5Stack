#include "EEPROM.h"
#include <M5StickC.h>
#include <stdio.h>
#include <stdlib.h>
#include "input_handler.h"
#include "constants.h"




/**
 * Does the necessary actions depending on the button(s) pressed and the tilt of the M5Stack.
 */
uint8_t handle_input(struct game_state *game_state, accelerometer_data accelerometer_data) {
    uint8_t change = 0;

    if (M5.BtnA.wasPressed() && M5.BtnB.wasPressed()) {
        write_data(game_state);
        return 0;

    } else if (M5.BtnB.wasPressed()) {
        load_data(game_state);
        return 0;
    } else {
        tetromino_state tetromino = game_state->tetromino_state;

        if (accelerometer_data.x > ACC_MIN_LEFT) {
            tetromino.column_in_field--;
            change = 1;
            delay(100);

        } else if (accelerometer_data.x < ACC_MIN_RIGHT) {
            tetromino.column_in_field++;
            change = 1;
            delay(100);
        }

        if (accelerometer_data.y > ACC_MIN_FAST) {
            game_state->speed.counter += SPEED_INCREASE;

        } else if (accelerometer_data.y < ACC_MIN_SLOW) {
            if (game_state->speed.counter > 0) {
                game_state->speed.counter--;
            }
        }

        if (M5.BtnA.wasPressed() && !M5.BtnB.wasPressed()) {
            tetromino.rotation = (tetromino.rotation + 1) % 4;
            change = 1;
        }

        uint8_t piece_orientation = piece_fits(tetromino, game_state, FIELD_WIDTH, FIELD_HEIGHT);

        if (change && (piece_orientation == CAN_MOVE)) {
            game_state->tetromino_state = tetromino;
            return 1;
        } else if (game_state->bomb.active && !(piece_orientation == OUTSIDE_WIDTH)) {
            explode(game_state, game_state->tetromino_state.column_in_field, game_state->tetromino_state.row_in_field);
            spawn_piece(game_state);
            return 1;
        } else {
            return 0;
        }
    }
}
