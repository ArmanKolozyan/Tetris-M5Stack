#include "EEPROM.h"
#include <M5StickC.h>
#include <stdio.h>
#include <stdlib.h>
#include "tetrominos.h"
#include "input_handler.h"
#include "installed_pieces.h"
#include "constants.h"



/**
 * All the different game_modes that the player, at the beginning of a game session, can choose.
 */
enum game_mode : byte {
    no_mines_no_timed_blocks,
    mines_only,
    timed_blocks_only,
    mines_and_timed_blocks
};

/**
 * The speed at which the tetrominos are moved downwards.
 */
struct speed {
    uint8_t limit;
    uint8_t counter;
};




/**
 * Structure that gathers the whole state of the game together.
 */
struct game_state {
    struct double_node *field[FIELD_WIDTH * FIELD_HEIGHT];

    enum game_mode game_mode;

    struct tetromino_state tetromino_state;

    struct timed_block timed_block;

    struct bomb bomb;

    struct speed speed;

    struct double_node *active_pieces;

    uint score;
    uint8_t game_over;
    uint8_t current_id;
};









void setup() {
    M5.begin();
    M5.IMU.Init();
    Serial.begin(115200);
    Serial.flush();
    EEPROM.begin(MEM_SIZE);
    M5.Lcd.fillScreen(TFT_BLACK);
    Serial.begin(115200);
    Serial.flush();
}



void loop() {
    // Since the M5Stack has no clock, the random_generator cannot be seeded with time(NULL).
    // For that reason, the accelerometer is used here to obtain a somewhat random value.
    float random_seeder;
    M5.Imu.getAccelData(&random_seeder, &random_seeder, &random_seeder);
    srand(random_seeder);
    struct accelerometer_data accelerometer_data;
    struct game_state game_state;
    short old_column;
    short old_row;
    uint8_t moved;

    game_state.current_id = 0;
    game_state.timed_block.active = 0;

    M5.update();
    draw_menu(&game_state);

    for (uint8_t x = 0; x < FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
            field_set(game_state.field, y, x, NULL);
        }
    }
    game_state.active_pieces = NULL;
    spawn_piece(&game_state);

    while (!game_state.game_over) {
        M5.update(); // moet er gewoon standard staan
        delay(10);
        M5.Imu.getAccelData(&accelerometer_data.x, &accelerometer_data.y, &accelerometer_data.z);
        old_column = game_state.tetromino_state.column_in_field;
        old_row = game_state.tetromino_state.row_in_field;
        moved = handle_input(&game_state, accelerometer_data);
        game_state.speed.counter++;

        if (game_state.speed.counter > game_state.speed.limit) {
            game_state.speed.counter = 0;
            if (game_state.bomb.active) {
                if (possible_to_lower(&game_state)) {
                    game_state.tetromino_state.row_in_field++;
                } else {
                    explode(&game_state, game_state.tetromino_state.column_in_field, game_state.tetromino_state.row_in_field);
                    spawn_piece(&game_state);
                }
            } else if (possible_to_lower(&game_state)) {
                game_state.tetromino_state.row_in_field++;
            } else {
                game_state.speed.counter = 1;
                if (piece_fits(game_state.tetromino_state, &game_state, FIELD_WIDTH, FIELD_HEIGHT) != CAN_MOVE) {
                    game_state.game_over = 1;
                    break;
                }
                install_tetromino_in_field(&game_state);
                handle_full_lines(&game_state);

                M5.Lcd.fillScreen(TFT_BLACK);

                // Draw Field
                for (uint8_t x = 0; x < FIELD_WIDTH; x++)
                    for (uint8_t y = 0; y < FIELD_HEIGHT; y++) {
                        if (game_state.field[y * FIELD_WIDTH + x] != NULL) {
                            Serial.printf("doing");
                            M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(game_state.field[y * FIELD_WIDTH + x]->value.color));
                        } else
                            M5.Lcd.fillRect(8 * x, 8 * y, 8, 8, give_color(0));
                    }

                game_state.score += 12;
                spawn_piece(&game_state);
            }
        }

        if ((game_state.speed.counter == 0) || moved) {
            moved = 0;
            struct tetromino *tetromino = game_state.tetromino_state.tetromino;
            for (short block_x = 0; block_x < tetromino->length; block_x++)
                for (short block_y = 0; block_y < tetromino->length; block_y++) {
                    M5.Lcd.fillRect((old_column + block_x) * 8, (old_row + block_y) * 8, 8, 8, TFT_BLACK);
                }

            // Draw Current Piece
            for (uint8_t block_x = 0; block_x < tetromino->length; block_x++)
                for (uint8_t block_y = 0; block_y < tetromino->length; block_y++) {
                    uint8_t value = get_component(tetromino, block_y, block_x, game_state.tetromino_state.rotation);
                    if (value) {
                        M5.Lcd.fillRect((game_state.tetromino_state.column_in_field + block_x) * 8, (game_state.tetromino_state.row_in_field + block_y) * 8, 8, 8, give_color(value));
                    }
                }
        }
    }
}
