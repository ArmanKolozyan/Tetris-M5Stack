/**
 * The current state of the game can be saved by pressing the A and B buttons at the same time.
 */
void write_data(struct game_state *game_state) {

    uint address = 0;

    uint8_t game_mode;
    switch (game_state->game_mode) {
    case mines_and_timed_blocks:
        game_mode = 1;
    case no_mines_no_timed_blocks:
        game_mode = 2;
    case mines_only:
        game_mode = 3;
    case timed_blocks_only:
        game_mode = 4;
    }
    uint8_t shifted_game_mode = game_mode << 6;
    uint8_t shifted_game_over = game_state->game_over << 5;
    uint8_t shifted_bomb_active = game_state->bomb.active << 4;
    uint8_t shifted_timed_block_active = game_state->timed_block.active << 3;
    uint8_t encoded_1 = shifted_game_mode | shifted_game_over | shifted_bomb_active | shifted_timed_block_active | game_state->timed_block.index;

    Serial.printf("encoded: %u\n", encoded_1);
    EEPROM.writeByte(address, encoded_1);
    address++;

    // It is a variabel struct member so that a programmer can make changes to it in his game (i.e. to promote extensibility and adaptability).
    // In my implementation, however, the timed_block limit can be up to 7.

    uint8_t shifted_timed_block_limit = game_state->timed_block.limit << 5;
    uint8_t encoded_2 = shifted_timed_block_limit | game_state->speed.counter;
    EEPROM.writeByte(address, encoded_2);
    address++;

    // Also, technically, no maximum can be placed on the speed_counter and speed_limit.
    // They are variabel struct members so that a programmer can make it changes to it in his game,
    // for example, when introducing multiple levels (i.e. to promote extensibility and adaptability).
    // In my implementation, however, the counter can be up to 31 and the limit up to 30.

    EEPROM.writeByte(address, game_state->speed.limit);
    address++;

    // field pieces
    struct double_node *field_pieces = game_state->active_pieces;

    // A field with my type of tetrominos can contain up to 50 tetrominos.
    // That means 6 bits. Splitting it up with other information makes little sense and is also not possible.
    uint8_t amount = count_nodes(field_pieces);
    EEPROM.writeByte(address, amount);
    address++;

    uint8_t shifted_color;
    uint8_t shifted_blocks_left;
    uint8_t encoded_3;

    while (field_pieces != NULL) {
        // Another programmer could introduce many more colors, but in my case there are only 9 different colors. -> 4 bits
        // A tetromino could contain a maximum of 4 blocks. -> 3 bits
        shifted_color = field_pieces->value.color << 3;
        shifted_blocks_left = field_pieces->value.blocks_left;
        encoded_3 = shifted_color | shifted_blocks_left;
        EEPROM.writeByte(address, encoded_3);
        address++;
        for (uint8_t i = 0; i < field_pieces->value.free_index; i += 2) {
            // column can be between 0 and 9 -> 4 bits
            // row can be between 0 and 19 -> 5 bits
            // combining them would save memory, but that is not trvial in this case and the computational overhead would domen
            EEPROM.writeByte(address, field_pieces->value.positions[i]);
            address++;
            EEPROM.writeByte(address, field_pieces->value.positions[i + 1]);
            address++;
        }
        field_pieces = field_pieces->next;
    }

    // current tetromino

    // 9 different colors -> 4 bits
    // column can be between 0 and 9 -> 4 bits, together 1 byte
    uint8_t shifted_current_color = determine_color(game_state->tetromino_state.tetromino->blocks, game_state->tetromino_state.tetromino->length) << 4;
    uint8_t encoded_4 = shifted_current_color | (uint8_t)game_state->tetromino_state.column_in_field;
    EEPROM.writeByte(address, encoded_4);
    address++;

    // row can be between 0 and 19 -> 5 bits
    // rotation can be between 0 and 3 -> 2 bits, together 7 bits
    uint8_t shifted_current_row = (uint8_t)game_state->tetromino_state.row_in_field << 3;
    uint8_t encoded_5 = shifted_current_row | game_state->tetromino_state.rotation;
    EEPROM.writeByte(address, encoded_5);
    address++;

    // score
    EEPROM.writeUInt(address, game_state->score); // Sticking a limit on the score would not be a good idea.
    address += sizeof(uint32_t);

    EEPROM.commit();
}

/**
 * The saved state can be loaded by pressing the B button.
 */
void load_data(struct game_state *game_state) {

    uint address = 0;

    uint8_t encoded_1 = EEPROM.readByte(address);
    address++;
    uint8_t bitmask_game_mode = 0b11000000;
    uint8_t shifted_game_mode = encoded_1 & bitmask_game_mode;
    switch (shifted_game_mode >> 6) {
    case 1:
        game_state->game_mode = mines_and_timed_blocks;
    case 2:
        game_state->game_mode = no_mines_no_timed_blocks;
    case 3:
        game_state->game_mode = mines_only;
    case 4:
        game_state->game_mode = timed_blocks_only;
    }

    uint8_t bitmask_game_over = 0b00100000;
    uint8_t shifted_game_over = encoded_1 & bitmask_game_over;
    game_state->game_over = shifted_game_over >> 5;

    uint8_t bitmask_bomb_active = 0b00010000;
    uint8_t shifted_bomb_active = encoded_1 & bitmask_bomb_active;
    game_state->bomb.active = shifted_bomb_active >> 4;

    uint8_t bitmask_timed_block_active = 0b00001000;
    uint8_t shifted_timed_block_active = encoded_1 & bitmask_timed_block_active;
    game_state->timed_block.active = shifted_timed_block_active >> 3;

    uint8_t bitmask_timed_block_index = 0b00000111;
    uint8_t shifted_timed_block_index = encoded_1 & bitmask_timed_block_index;
    game_state->timed_block.index = shifted_timed_block_index;

    //(timed_block &) speed

    uint8_t encoded_2 = EEPROM.readByte(address);
    address++;
    uint8_t bitmask_timed_block_limit = 0b11100000;
    uint8_t shifted_timed_block_limit = encoded_2 & bitmask_timed_block_limit;
    game_state->timed_block.limit = shifted_timed_block_limit >> 5;

    uint8_t bitmask_speed_counter = 0b00011111;
    uint8_t shifted_speed_counter = encoded_2 & bitmask_speed_counter;
    game_state->speed.counter = shifted_speed_counter;

    game_state->speed.limit = EEPROM.readByte(address);
    address++;

    // field pieces
    for (uint8_t row = 0; row < FIELD_HEIGHT; row++) {
        for (uint8_t column = 0; column < FIELD_WIDTH; column++) {
            if (game_state->field[row * FIELD_WIDTH + column] != NULL) {
                game_state->active_pieces = delete_node(game_state->field[row * FIELD_WIDTH + column]->value.id, game_state->active_pieces);
                game_state->field[row * FIELD_WIDTH + column] = NULL;
            }
        }
    }
    uint8_t counter = EEPROM.readByte(address);
    address++;
    game_state->active_pieces = NULL;
    game_state->current_id = 1;

    uint8_t encoded_3;
    uint8_t bitmask_color = 0b01111000;
    uint8_t bitmask_blocks_left = 0b00000111;
    uint8_t shifted_color;
    uint8_t color;
    uint8_t blocks_left;

    for (uint8_t i = counter; i > 0; i--) {
        encoded_3 = EEPROM.readByte(address);
        address++;
        shifted_color = encoded_3 & bitmask_color;
        color = shifted_color >> 3;
        blocks_left = encoded_3 & bitmask_blocks_left;
        struct installed_piece_data value = create_installed_piece(game_state->current_id, color, blocks_left);
        game_state->active_pieces = insert_before(value, game_state->active_pieces);
        game_state->current_id++;
        for (uint8_t i = 0; i < game_state->active_pieces->value.blocks_left; i++) {
            uint8_t column = EEPROM.readByte(address);
            address++;
            uint8_t row = EEPROM.readByte(address);
            address++;
            game_state->field[row * FIELD_WIDTH + column] = game_state->active_pieces;
            uint8_t free_index = game_state->active_pieces->value.free_index;
            game_state->active_pieces->value.positions[free_index] = column;
            game_state->active_pieces->value.positions[free_index + 1] = row; // beter aparte functie hiervoor maken???
            game_state->active_pieces->value.free_index += 2;
        }
    }

    // current tetromino

    if ((determine_color(game_state->tetromino_state.tetromino->blocks, game_state->tetromino_state.tetromino->length)) == 9) {
        free(game_state->tetromino_state.tetromino->blocks);
        free(game_state->tetromino_state.tetromino);
    }

    uint8_t encoded_4 = EEPROM.readByte(address);
    address++;
    uint8_t bitmask_current_color = 0b11110000;
    uint8_t shifted_current_color = encoded_4 & bitmask_current_color;
    uint8_t current_color = shifted_current_color >> 4;

    if (current_color == 9) {
        make_timed_block(game_state, game_state->timed_block.index);
    } else {
        game_state->tetromino_state.tetromino = &TETROMINOS[current_color - 1];
    }

    uint8_t bitmask_current_column = 0b00001111;
    game_state->tetromino_state.column_in_field = encoded_4 & bitmask_current_column;

    uint8_t encoded_5 = EEPROM.readByte(address);
    address++;
    uint8_t bitmask_current_row = 0b11111000;
    uint8_t shifted_current_row = encoded_5 & bitmask_current_row;
    game_state->tetromino_state.row_in_field = shifted_current_row >> 3;
    uint8_t bitmask_current_rotation = 0b00000011;
    game_state->tetromino_state.rotation = encoded_5 & bitmask_current_rotation;

    // score
    game_state->score = EEPROM.readUInt(address);
    Serial.printf("score receiving: %u\n", game_state->score);
    address += sizeof(uint32_t);
}