
/* Copyright Nate Carson 2012 */

#include <stddef.h>
#include <stdio.h>

/* pg */
#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

#include "utils/array.h"
#include "utils/lsyscache.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"

PG_FUNCTION_INFO_V1(chess_cmp);
PG_FUNCTION_INFO_V1(chess_cmp_eq);
PG_FUNCTION_INFO_V1(chess_cmp_neq);
PG_FUNCTION_INFO_V1(chess_cmp_gt);
PG_FUNCTION_INFO_V1(chess_cmp_lt);
PG_FUNCTION_INFO_V1(chess_cmp_lteq);
PG_FUNCTION_INFO_V1(chess_cmp_gteq);

PG_FUNCTION_INFO_V1(square_in);
PG_FUNCTION_INFO_V1(square_out);
PG_FUNCTION_INFO_V1(square_to_int);
PG_FUNCTION_INFO_V1(int_to_square);

PG_FUNCTION_INFO_V1(piece_in);
PG_FUNCTION_INFO_V1(piece_out);

PG_FUNCTION_INFO_V1(board_in);
PG_FUNCTION_INFO_V1(board_out);

/*
   PG_FUNCTION_INFO_V1(piecesquare_in);
   PG_FUNCTION_INFO_V1(piecesquare_out);
   PG_FUNCTION_INFO_V1(piecesquare_square);
   PG_FUNCTION_INFO_V1(piecesquare_piece);
   */

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

const int	MAX_FEN = 100;
const int	MAX_PIECES = 32;
const int	MAX_MOVES = 192;
const int	MAX_SAN = 10;

// postgresql type names
const char TYPE_PIECESQUARE[]  = "piecesquare";
const char TYPE_SQUARE[] = "square";
const char TYPE_PIECE[] = "piece";
const char TYPE_BOARD[] = "board";

#define WHITE_PAWN 0x0        // 0 | White Pawn (normal)                         |
#define WHITE_ROOK 0x1        // 1 | White Rook (has moved)                      |
#define WHITE_KNIGHT 0x02     // 2 | White Knight                                |
#define WHITE_BISHOP 0x3      // 3 | White Bishop                                |
#define WHITE_QUEEN 0x4       // 4 | White Queen                                 |
#define WHITE_KING_MOVE 0x5   // 5 | White King; White to move next              |
#define WHITE_KING 0x6        // 6 | White King                                  |
#define WHITE_SPECIAL 0x7     // 7 | White Rook (pre castle) / Pawn (en Passant) |
#define BLACK_PAWN 0x8        // 8 | Black Pawn (normal)                         |
#define BLACK_ROOK 0x9        // 9 | Black Rook (has moved)                      |
#define BLACK_KNIGHT 0xA      // A | Black Knight                                |
#define BLACK_BISHOP 0xB      // B | Black Bishop                                |
#define BLACK_QUEEN  0xC      // C | Black Queen                                 |
#define BLACK_KING 0xD        // D | Black King; Black to move next              |
#define BLACK_KING_MOVE 0xE   // E | Black King                                  |
#define BLACK_SPECIAL 0xF     // F | Black Rook (pre castle) / Pawn (en Passant) |

typedef struct {
    int32 vl_len;
    int32 data[FLEXIBLE_ARRAY_MEMBER];
} board;

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define NOTICE1(msg) ereport(NOTICE, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("%s", msg)))
#define NOTICE2(msg, arg) ereport(NOTICE, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(msg, arg)))
#define NOTICE3(msg, arg1, arg2) ereport(NOTICE, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(msg, arg1, arg2)))

#define ERROR1(msg) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("%s", msg)))
#define ERROR2(msg, arg) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(msg, arg)))
#define ERROR3(msg, arg1, arg2) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(msg, arg1, arg2)))

#define BAD_TYPE_IN(type, input) ereport( \
			ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), \
			errmsg("invalid input syntax for %s: \"%s\"", type, input)))

#define BAD_TYPE_OUT(type, input) ereport( \
			ERROR, (errcode(ERRCODE_DATA_CORRUPTED), \
			errmsg("corrupt internal data for %s: \"%d\"", type, input)))


/*

chess=# select to_hex(idx), idx::bit(4)  from generate_series(0, 15) as idx;
 to_hex | idx  
--------+------
 0	  | 0000
 1	  | 0001
 2	  | 0010
 3	  | 0011
 4	  | 0100
 5	  | 0101
 6	  | 0110
 7	  | 0111
 8	  | 1000
 9	  | 1001
 a	  | 1010
 b	  | 1011
 c	  | 1100
 d	  | 1101
 e	  | 1110
 f	  | 1111
(16 rows)


*/



/********************************************************
* 		util
********************************************************/


/*
static ArrayType * make_array(const char *typname, size_t size, Datum * data)
{
	ArrayType	*result;
	Oid			element_type = TypenameGetTypid(typname);
	if (!OidIsValid(element_type))
		elog(ERROR, "could not find '%s' type.", typname);

	int16		typlen;
	bool		typbyval;
	char		typalign;

	get_typlenbyvalalign(element_type, &typlen, &typbyval, &typalign);

	result = construct_array(data, size, element_type, typlen, typbyval, typalign);
	if (!result)
		elog(ERROR, "constructing array failed");
	return result;
}
*/

/********************************************************
* 		operators
********************************************************/

static int
_cmp_internal(int a, int b)
{
	// Our biggest type is int16 so we should be able to promote
	// and use this function for everything.
	
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}


Datum
chess_cmp_eq(PG_FUNCTION_ARGS)
{
    char    	a = PG_GETARG_CHAR(0);
    char    	b = PG_GETARG_CHAR(1);

    PG_RETURN_BOOL(_cmp_internal(a, b) == 0);
}

Datum
chess_cmp_neq(PG_FUNCTION_ARGS)
{
    int			a = (int) PG_GETARG_INT32(0);
    int			b = (int) PG_GETARG_INT32(1);

    PG_RETURN_BOOL(_cmp_internal(a, b) != 0);
}

Datum
chess_cmp_lt(PG_FUNCTION_ARGS)
{
    int			a = (int) PG_GETARG_INT32(0);
    int			b = (int) PG_GETARG_INT32(1);

    PG_RETURN_BOOL(_cmp_internal(a, b) < 0);
}

Datum
chess_cmp_gt(PG_FUNCTION_ARGS)
{
    int			a = (int) PG_GETARG_INT32(0);
    int			b = (int) PG_GETARG_INT32(1);

    PG_RETURN_BOOL(_cmp_internal(a, b) > 0);
}

Datum
chess_cmp_lteq(PG_FUNCTION_ARGS)
{
    int			a = (int) PG_GETARG_INT32(0);
    int			b = (int) PG_GETARG_INT32(1);

    PG_RETURN_BOOL(_cmp_internal(a, b) <= 0);
}

Datum
chess_cmp_gteq(PG_FUNCTION_ARGS)
{
    int			a = (int) PG_GETARG_INT32(0);
    int			b = (int) PG_GETARG_INT32(1);

    PG_RETURN_BOOL(_cmp_internal(a, b) >= 0);
}

/********************************************************
 * 		fen
 ********************************************************/

/* https://codegolf.stackexchange.com/questions/19397/smallest-chess-board-compression
 * second answer : 192 bits
 */

/*
static char *_int64_to_bin(int64 a, char *buffer, int buf_size) {
    buffer += (buf_size - 1);

    for (int i = 63; i >= 0; i--) {
        *buffer-- = (a & 1) + '0';

        a >>= 1;
    }

    return buffer;
}
*/


Datum
board_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);
    board           *result;
    char            c, p='\0';
    int64           bitboard=0;
	int32			pieces[4];
    int             i=0, j=63, k=0;
    bool            done=false;
    size_t          s;
	//char			*debug = palloc(64);
	
    if (strlen(str) > MAX_FEN)
        ERROR1("fen string too long");

    while (str[i] != '\0') {

        //first fallthrough switch for bitboard
        switch (c=str[i]) {
            case 'p':
            case 'n':
            case 'b':
            case 'r':
            case 'q':
            case 'k':
            case 'P':
            case 'N':
            case 'B':
            case 'R':
            case 'Q':
            case 'K':
                bitboard |= (1Ul << (j--));
				//_int64_to_bin(bitboard, debug, 64);
				//NOTICE3("%c %s", str[i], debug);
                // second switch for pieces
                switch (c=str[i]) {
                    case 'p':
                        p = BLACK_PAWN;
                        break;
                    case 'n':
                        p = BLACK_KNIGHT;
                        break;
                    case 'b':
                        p = BLACK_BISHOP;
                        break;
                    case 'r':
                        p = BLACK_ROOK;
                        break;
                    case 'q':
                        p = BLACK_QUEEN;
                        break;
                    case 'k':
                        p = BLACK_KING;
                        break;
                    case 'P':
                        p = WHITE_PAWN;
                        break;
                    case 'N':
                        p = WHITE_KNIGHT;
                        break;
                    case 'B':
                        p = WHITE_BISHOP;
                        break;
                    case 'R':
                        p = WHITE_ROOK;
                        break;
                    case 'Q':
                        p = WHITE_QUEEN;
                        break;
                    case 'K':
                        p = WHITE_KING;
                        break;
                }
                pieces[k/8] |= (p << ((7-(k%8))*4));
                k++;
                if (k>MAX_PIECES)
                    ERROR1("too many pieces in fen");
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
			case '6':
            case '7':
            case '8':
                j -= (c - '0');
            case '/':
                    break;
            case ' ':
                    done=true;
                    break;
            default:
                BAD_TYPE_IN(TYPE_BOARD, str);
                break;
        }

        //https://stackoverflow.com/questions/2810280/how-to-store-a-64-bit-integer-in-two-32-bit-integers-and-convert-back-again
		i++;

		if (done) 
			break;
        if (j<-1)
            ERROR1("FEN board is too long");
    }

    if (j>-1)
        ERROR1("FEN board is too short");

    // size 8 piece per int, add one byte if were not on an oct then two more for the bitboard
    s = sizeof(int32)*(k/8 +(k%8?1:0)) + sizeof(int32)*2;
    result = (board*)palloc(s);
    SET_VARSIZE(result, s);
    // split up the bitboards
    result->data[0] = (int32)((bitboard & 0xFFFFFFFF00000000LL) >> 32);
    result->data[1] = (int32)(bitboard & 0xFFFFFFFFLL);
    // add the pieces
    for (i=0; i<k; i++)
        result->data[i] = pieces[i];

    NOTICE1("0");
    PG_RETURN_POINTER(result);
}

Datum
board_out(PG_FUNCTION_ARGS)
{

    //_int_gist.c:    GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);

    board           *b = (board *) PG_GETARG_POINTER(0);
    char            *str = palloc(100);
    char            *result;
    int64           bitboard = ((int64) b->data[0]) << 32 | b->data[1];
    int             i=0, j=0, empties=0;

    for (i=63; i>-1; i--) {
        NOTICE2("bitboard %d", i);
        if CHECK_BIT(bitboard, i) {
            if (empties) {
                switch (empties) {
                    case 1: str[j++]= '1'; break;
                    case 2: str[j++] = '2'; break;
                    case 3: str[j++] = '3'; break;
                    case 4: str[j++] = '4'; break;
                    case 5: str[j++] = '5'; break;
                    case 6: str[j++] = '6'; break;
                    case 7: str[j++] = '7'; break;
                    case 8: str[j++] = '8'; break;
                    default:
                        str[j++] = '*'; //FIXME
                        break;
                }
            }
            empties = 0;
            switch(b->data[j/8] >> j*4 & 0x0000000F) {
                case BLACK_PAWN:    str[j++] = 'p'; break;
                case BLACK_KNIGHT:  str[j++] = 'n'; break;
                case BLACK_BISHOP:  str[j++] = 'b'; break;
                case BLACK_ROOK:    str[j++] = 'r'; break;
                case BLACK_QUEEN:   str[j++] = 'q'; break;
                case BLACK_KING:    str[j++] = 'k'; break;
                case WHITE_PAWN:    str[j++] = 'P'; break;
                case WHITE_KNIGHT:  str[j++] = 'N'; break;
                case WHITE_BISHOP:  str[j++] = 'B'; break;
                case WHITE_ROOK:    str[j++] = 'R'; break;
                case WHITE_QUEEN:   str[j++] = 'Q'; break;
                case WHITE_KING:    str[j++] = 'K'; break;

                default:
                    BAD_TYPE_OUT(TYPE_BOARD, b->data[j/8]);
                    break;
            }
            NOTICE2("piece %c", str[j-1]);
        } else {
            empties += 1;
        }

    }
    str[j++] = '\0';
    result = (char *)palloc(j);
    memcpy(result, str, j);

    PG_RETURN_CSTRING(result);
}




/********************************************************
* 		piece
********************************************************/

/*
static char *_make_str_piece(char piece) 
{
	char *msg = (char *) palloc(2);
	msg[0] = piece; 
	msg[1]='\0';
	return msg;
}
*/

static char *_make_str_square(char file, char rank) 
{
	char *msg = (char *) palloc(3);
	msg[0] = file;
	msg[1] = rank;
	msg[2] = '\0';
	return msg;
}

/*
static char _piece_in(char piece)
{
}
*/

static char	_piece_out(char piece)
{

	switch (piece) {
		case 'p':
		case 'n':
		case 'b':
		case 'r':
		case 'q':
		case 'k':
		case 'P':
		case 'N':
		case 'B':
		case 'R':
		case 'Q':
		case 'K':
			break;

		default:
			BAD_TYPE_OUT(TYPE_PIECE, piece);
			break;
	}
	return piece;
}


Datum
piece_in(PG_FUNCTION_ARGS)
{
	char 			*str = PG_GETARG_CSTRING(0);

	if (strlen(str) != 1)
		BAD_TYPE_IN(TYPE_PIECE, str);

	switch (str[0]) {
		case 'p':
		case 'n':
		case 'b':
		case 'r':
		case 'q':
		case 'k':
		case 'P':
		case 'N':
		case 'B':
		case 'R':
		case 'Q':
		case 'K':
			break;

		default:
			BAD_TYPE_IN(TYPE_PIECE, str);
			break;
	}

	PG_RETURN_CHAR(str[0]);
}


Datum
piece_out(PG_FUNCTION_ARGS)
{
	char			piece = PG_GETARG_CHAR(0);
	char			*result = (char *) palloc(2);

	result[0] = _piece_out(piece);
	result[1] = '\0';

	PG_RETURN_CSTRING(result);
}


/********************************************************
* 		square
********************************************************/

static char _square_in(char file, char rank)
{
	char			c=0;

	if (file < 'a' || file > 'h') 
		BAD_TYPE_IN(TYPE_SQUARE, _make_str_square(file, rank));
	else if (rank < '1' ||  rank > '8')
		BAD_TYPE_IN(TYPE_SQUARE, _make_str_square(file, rank));
	
	c = ((file - 'a') * 8) + (rank - '1');

	if (c < 0 || c > 63)
		ERROR2("bad conversion for square %s" , _make_str_square(file, rank));
	return c;
}

static void _square_out(char c, char *str)
{

	if (c < 0 || c > 63)
		BAD_TYPE_OUT(TYPE_SQUARE, c);

	switch (c / 8) {
		case 0: str[0]='a'; break;
		case 1: str[0]='b'; break;
		case 2: str[0]='c'; break;
		case 3: str[0]='d'; break;
		case 4: str[0]='e'; break;
		case 5: str[0]='f'; break;
		case 6: str[0]='g'; break;
		case 7: str[0]='h'; break;

		default: ERROR2("bad switch statement %d", c);
	}

	switch (c % 8) {
		case 0: str[1]='1'; break;
		case 1: str[1]='2'; break;
		case 2: str[1]='3'; break;
		case 3: str[1]='4'; break;
		case 4: str[1]='5'; break;
		case 5: str[1]='6'; break;
		case 6: str[1]='7'; break;
		case 7: str[1]='8'; break;

		default: ERROR2("bad switch statement %d", c);
	}
}


Datum
square_in(PG_FUNCTION_ARGS)
{
	char 			*str = PG_GETARG_CSTRING(0);

	if (strlen(str) != 2)
		BAD_TYPE_IN(TYPE_SQUARE, str); 

	PG_RETURN_CHAR(_square_in(str[0], str[1]));
}


Datum
square_out(PG_FUNCTION_ARGS)
{
	char			square = PG_GETARG_CHAR(0);
	char			*result = (char *) palloc(3);

	_square_out(square, result);
	result[2] = '\0';

	PG_RETURN_CSTRING(result);
}

Datum
square_to_int(PG_FUNCTION_ARGS)
{
    char			square = PG_GETARG_CHAR(0);
    PG_RETURN_INT32((int32)square);
}

Datum
int_to_square(PG_FUNCTION_ARGS)
{
    char            c = (char)PG_GETARG_INT32(0);
    char			*result = (char *) palloc(3);

    if (c < 0 || c > 63)
        ERROR1("value must be in between 0 and 63");
    PG_RETURN_CHAR(c);

    _square_out(c, result);
    result[2] = '\0';
    PG_RETURN_CSTRING(result);
}


/* squares 2^6 piece 2^3 color 2^1*/
/********************************************************
* 		piecesquare
********************************************************/

/*
static uint16 _piecesquare_in(ChessPiece piece, ChessSquare square)
{
	uint16			result =0;
	result = piece << 6;
	result = result | square;
	return result;
}

static ChessPiece _piecesquare_piece(uint16 piecesquare)
{
	ChessPiece		piece = piecesquare >> 6 & 15;

    if (!(piece >= CHESS_PIECE_WHITE_PAWN && piece <= CHESS_PIECE_BLACK_KING))
		BAD_TYPE_OUT(TYPE_PIECE, piece);
	return piece;
}

static ChessPiece _piecesquare_square(uint16 piecesquare)
{
	return piecesquare & 63;
}



Datum
piecesquare_in(PG_FUNCTION_ARGS)
{
	char 			*str = PG_GETARG_CSTRING(0);
	ChessPiece		piece;
	ChessSquare		square;
	
	if (strlen(str) != 3)
		BAD_TYPE_IN(TYPE_PIECE, str);
	
	piece = _piece_in(str);
	square = _square_in(++str);
	
	PG_RETURN_INT16(_piecesquare_in(piece, square));
}


Datum
piecesquare_out(PG_FUNCTION_ARGS)
{
	uint16			piecesquare = PG_GETARG_UINT16(0);
	char			*result = (char *) palloc(4);
	ChessPiece		piece = _piecesquare_piece(piecesquare);
	ChessSquare		square = _piecesquare_square(piecesquare);

	result[0] = _piece_out(piece);
	_square_out(square, result+1);
	result[3] = '\0';

	PG_RETURN_CSTRING(result);
}


Datum
piecesquare_square(PG_FUNCTION_ARGS)
{
	uint16			piecesquare = PG_GETARG_UINT16(0);
	PG_RETURN_CHAR(_piecesquare_square(piecesquare));
}


Datum
piecesquare_piece(PG_FUNCTION_ARGS)
{
	uint16			piecesquare = PG_GETARG_UINT16(0);
	PG_RETURN_CHAR(_piecesquare_piece(piecesquare));
}

*/

