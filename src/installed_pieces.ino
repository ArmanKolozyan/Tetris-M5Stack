#include "EEPROM.h"
#include <M5StickC.h>
#include <stdio.h>
#include <stdlib.h>
#include "installed_pieces.h"
#include "constants.h"





Double_Node *insert_before(installed_piece_data value, Double_Node *q) {
    Double_Node *p = (Double_Node *)malloc(sizeof(Double_Node));
    p->value = value;
    p->next = q;
    if (q != NULL) {
        Double_Node *prev_node = q->prev;
        q->prev = p;
        p->prev = prev_node;
        if (prev_node != NULL) {
            prev_node->next = p;
        }
    } else {
        p->prev = NULL;
    }
    return p;
}

Double_Node *delete_node(uint id, Double_Node *start) {
    Double_Node *p;
    p = start;
    while (p != NULL && (p->value.id != id)) {
        p = p->next;
    }
    if (p != NULL) {
        Double_Node *next = p->next;
        Double_Node *prev = p->prev;
        if (prev != NULL) {
            prev->next = next;
            if (next != NULL) {
                next->prev = prev;
            }
        } else {
            start = p->next;
            if (next != NULL) {
                next->prev = NULL;
            }
        }
        free(p->value.positions);
        free(p);
    }
    return start;
}

uint8_t count_nodes(Double_Node *start) {
    Double_Node *p;
    p = start;
    uint8_t i = 0;

    while (p != NULL) {
        p = p->next;
        i++;
    }

    return i;
}

struct installed_piece_data create_installed_piece(uint id, uint8_t color, uint8_t blocks) {

    struct installed_piece_data s;

    // Dynamically allocating memory according to blocks_left
    s.positions = (uint8_t *)malloc(sizeof(uint8_t) * 2 * blocks); // LET OP: GEBRUIKMAKEN VAN POSITIE STRUCTS!!

    s.id = id;
    s.color = color;
    s.blocks_left = blocks;
    s.free_index = 0;

    return s;
}


/**
 * Replaces the last position with the position to be deleted and decrements the free_index.
 */
void delete_position(installed_piece_data *installed_piece_data, uint8_t x, uint8_t y) {

    uint8_t *positions = installed_piece_data->positions;

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        if ((positions[i] == x) && (positions[i + 1] == y)) {
            positions[i] = positions[installed_piece_data->free_index - 2];
            positions[i + 1] = positions[installed_piece_data->free_index - 1];
            installed_piece_data->free_index -= 2;
            break;
        }
    }
    installed_piece_data->blocks_left--;
}

/**
 * Replaces the first given position with the second given position.
 */
void change_position(installed_piece_data *installed_piece_data, uint8_t x, uint8_t y, uint8_t new_x, uint8_t new_y) {

    uint8_t *positions = installed_piece_data->positions;

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        Serial.printf("POSITION BEFORE: %i\n", positions[i]);
    }

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        if ((positions[i] == x) && (positions[i + 1] == y)) {
            positions[i] = new_x;
            positions[i + 1] = new_y;
            break;
        }
    }

    for (uint8_t i = 0; i < installed_piece_data->blocks_left * 2; i++) {
        Serial.printf("POSITION AFTER: %i\n", positions[i]);
    }
}
