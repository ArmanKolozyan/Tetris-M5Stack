#ifndef installed_pieces
#define installed_pieces

/**
 * A tetromino that was installed in the playing field because it could no longer move.
 * The positions of the components of such a tetromino in the playing field are tracked to facilitate writing out and loading in the state of the game.
 * A free_index is provided to indicate the first free position in such a poisitions array, since positions can be both deleted and added.
 * Thus, such a tetromino is also flexible in its components (for possible further extensions).
 * Since the size of the array is not known in advance, it is allocated dynamically.
 */
struct installed_piece_data {
    uint id; // to distinguish between different installed tetrominos
    uint8_t color;
    uint8_t blocks_left; // to give the player bonus points when all blocks of a tetromino are cleared
    uint8_t free_index;
    uint8_t *positions;
};

/**
 * Each "cell" in the field must keep certain information about the tetromino it is part of, namely the installed_piece_data.
 * For this reason, each block in the playfield contains a pointer to a parent node, which contains the necessary information.
 * The data structure in which this information is maintained is a double-linked list, with installed_piece_data as its values.
 */
typedef struct double_node {
    struct installed_piece_data value;
    struct double_node *prev;
    struct double_node *next;
} Double_Node;


Double_Node *delete_node(uint id, Double_Node *start);
Double_Node *insert_before(installed_piece_data value, Double_Node *q);

#endif