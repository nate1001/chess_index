
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

#define EXTRA_DEBUG 1

PG_FUNCTION_INFO_V1(char_to_int);

PG_FUNCTION_INFO_V1(board_in);
PG_FUNCTION_INFO_V1(board_out);
PG_FUNCTION_INFO_V1(board_cmp);
PG_FUNCTION_INFO_V1(board_eq);
PG_FUNCTION_INFO_V1(board_ne);
PG_FUNCTION_INFO_V1(board_lt);
PG_FUNCTION_INFO_V1(board_gt);
PG_FUNCTION_INFO_V1(board_le);
PG_FUNCTION_INFO_V1(board_ge);
PG_FUNCTION_INFO_V1(board_hash);
PG_FUNCTION_INFO_V1(piece_count);
PG_FUNCTION_INFO_V1(board_go);

PG_FUNCTION_INFO_V1(square_in);
PG_FUNCTION_INFO_V1(square_out);
PG_FUNCTION_INFO_V1(int_to_square);

PG_FUNCTION_INFO_V1(piece_in);
PG_FUNCTION_INFO_V1(piece_out);

PG_FUNCTION_INFO_V1(cfile_in);
PG_FUNCTION_INFO_V1(cfile_out);
PG_FUNCTION_INFO_V1(square_to_cfile);

PG_FUNCTION_INFO_V1(rank_in);
PG_FUNCTION_INFO_V1(rank_out);
PG_FUNCTION_INFO_V1(square_to_rank);

PG_FUNCTION_INFO_V1(diagonal_in);
PG_FUNCTION_INFO_V1(diagonal_out);
PG_FUNCTION_INFO_V1(square_to_diagonal);

PG_FUNCTION_INFO_V1(adiagonal_in);
PG_FUNCTION_INFO_V1(adiagonal_out);
PG_FUNCTION_INFO_V1(square_to_adiagonal);


#define CHECK_BIT(board, k) ((1ul << k) & board)
#define GET_PIECE(pieces, k) k%2 ? pieces[k/2] & 0x0f : (pieces[k/2] & 0xf0) >> 4
#define SET_PIECE(pieces, k, v) pieces[k/2] = k%2 ? ( (pieces[k/2] & 0xF0) | (v & 0xF)) : ((pieces[k/2] & 0x0F) | (v & 0xF) << 4)
#define SET_BOARD(board, k) board |= (1Ul << (k--));
#define TO_SQUARE_IDX(i)  (i/8)*8 + (8 - i%8) - 1;
#define CHAR_CFILE(s) 'a' + TO_FILE(s)
#define CHAR_RANK(s) '1' + TO_RANK(s)
#define MAKE_SQUARE(file, rank, str) {str[0]=file; str[1]=rank;}
#define TO_RANK(s) s/8
#define TO_FILE(s) s%8

#define MAX_FEN 100
#define MAX_PIECES 32
#define MAX_MOVES 192
#define MAX_SAN 10
#define BOARD_SIZE 64
#define RANK_SIZE 8

#define WHITE_PAWN 0x0        // 0 | White Pawn (normal)                         |
#define WHITE_ROOK 0x1        // 1 | White Rook (has moved)                      |
#define WHITE_KNIGHT 0x02     // 2 | White Knight                                |
#define WHITE_BISHOP 0x3      // 3 | White Bishop                                |
#define WHITE_QUEEN 0x4       // 4 | White Queen                                 |
//#define WHITE_KING_MOVE 0x5   // 5 | White King; White to move next              |
#define WHITE_KING 0x6        // 6 | White King                                  |
//#define WHITE_SPECIAL 0x7     // 7 | White Rook (pre castle) / Pawn (en Passant) |
#define BLACK_PAWN 0x8        // 8 | Black Pawn (normal)                         |
#define BLACK_ROOK 0x9        // 9 | Black Rook (has moved)                      |
#define BLACK_KNIGHT 0xA      // A | Black Knight                                |
#define BLACK_BISHOP 0xB      // B | Black Bishop                                |
#define BLACK_QUEEN  0xC      // C | Black Queen                                 |
#define BLACK_KING 0xD        // D | Black King; Black to move next              |
//#define BLACK_KING_MOVE 0xE   // E | Black King                                  |
//#define BLACK_SPECIAL 0xF     // F | Black Rook (pre castle) / Pawn (en Passant) |

/*
 * base-size -> 14: 8 byte bitboard + 4 byte struct length (required by pg) + 2 for state
 * min size ->  16: 3 pieces of 4 bit nibbles = 2 bytes
 * max size ->  30: 32 pieces of 4 bit nibbles = 16 bytes
 *
 * reality of alignment 16-32
 */

typedef struct {
    int32           vl_len;
    unsigned int    whitesgo : 1;
    unsigned int    pcount : 6; //0-32
    int             enpassant : 7; // this could be reduced to side and file - 4 bits
    unsigned int    wk : 1;
    unsigned int    wq : 1;
    unsigned int    bk : 1;
    unsigned int    bq : 1;
    unsigned int    _unused: 14;
    int64           board;
#ifdef EXTRA_DEBUG
	char			orig_fen[MAX_FEN];
#endif
    unsigned char   pieces[FLEXIBLE_ARRAY_MEMBER];
} Board;

static char *_square_out(char c, char *str);
static int _board_fen(const Board * b, char * str);

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define CH_NOTICE(...) ereport(NOTICE, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(__VA_ARGS__)))
#define CH_ERROR(...) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(__VA_ARGS__)))
#define CH_DEBUG5(...) ereport(DEBUG5, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(__VA_ARGS__))) // most detail
#define CH_DEBUG4(...) ereport(DEBUG4, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(__VA_ARGS__)))
#define CH_DEBUG3(...) ereport(DEBUG3, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(__VA_ARGS__)))
#define CH_DEBUG2(...) ereport(DEBUG2, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(__VA_ARGS__)))
#define CH_DEBUG1(...) ereport(DEBUG1, (errcode(ERRCODE_INTERNAL_ERROR), errmsg(__VA_ARGS__))) // least detail

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

/*
 * 8    56      57      58      59      60      61      62      63
 * 7    48      49      50      51      52      53      54      55
 * 6    40      41      42      43      44      45      46      47
 * 5    32      33      34      35      36      37      38      39 
 * 4    24      25      26      27      28      29      30      31
 * 3    16      17      18      19      20      21      22      23
 * 2    08      09      10      11      12      13      14      15
 * 1    00      01      02      03      04      05      06      07
 *
 *      a       b       c       d       e       f       g       h   
 */


/********************************************************
* 		util
********************************************************/

/*
static ArrayType * make_array( char *typname, size_t size, Datum * data)
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

//http://www.cse.yorku.ca/~oz/hash.html
static unsigned int _sdbm_hash(str)
unsigned char *str;
{
	unsigned long hash = 0;
	int c;

	while ((c = *str++))
		hash = c + (hash << 6) + (hash << 16) - hash;
	return hash;
}

#ifdef EXTRA_DEBUG
static void debug_bitboard(int64 a) {

    char            *str = (char *) palloc(BOARD_SIZE+1);
    int             cnt=BOARD_SIZE-1;
    int64           b = a;

    str += (BOARD_SIZE - 1);
	while (cnt >=0) {
          str[cnt] = (b & 1) + '0';
          b >>= 1;
	     cnt--;
	}
	str[BOARD_SIZE] = '\0';
	CH_DEBUG5("bitboard: %ld: |%s|", a, str);
}
#endif
 
Datum
char_to_int(PG_FUNCTION_ARGS)
{
    char			c = PG_GETARG_CHAR(0);
    PG_RETURN_INT32((int32)c);
}

/********************************************************
 * 		files
 ********************************************************/

static char _cfile_in(char f) 
{
    if (f < 'a' || f > 'h') 
        CH_ERROR("chess file not in range %c", f);
    return f - 'a';
}

    Datum
square_to_cfile(PG_FUNCTION_ARGS)
{
    char 			s = PG_GETARG_CHAR(0);
    PG_RETURN_CHAR(TO_FILE(s));
}

    Datum
cfile_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);
    if (strlen(str) != 2)
        BAD_TYPE_IN("file", str); 
    PG_RETURN_CHAR(_cfile_in(str[0]));
}

    Datum
cfile_out(PG_FUNCTION_ARGS)
{
    char 			f = PG_GETARG_CHAR(0);
    char            *result = palloc(2);

    result[0] = f + 'a';
    result[1] = '\0';

    PG_RETURN_CSTRING(result);
}


/********************************************************
 * 		ranks
 ********************************************************/

static char _rank_in(char r) 
{
    if (r < '1' || r > '8') 
        CH_ERROR("chess rank '%c' not in range", r);
    return r - '1';
}

    Datum
square_to_rank(PG_FUNCTION_ARGS)
{
    char 			s = PG_GETARG_CHAR(0);
    PG_RETURN_CHAR(TO_RANK(s));
}

    Datum
rank_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);
    if (strlen(str) != 2)
        BAD_TYPE_IN("rank", str); 
    PG_RETURN_CHAR(_rank_in(str[1]));
}

    Datum
rank_out(PG_FUNCTION_ARGS)
{
    char 			f = PG_GETARG_CHAR(0);
    char            *result = palloc(2);

    result[0] = f + '1';
    result[1] = '\0';

    PG_RETURN_CSTRING(result);
}


/********************************************************
 * 		square
 ********************************************************/

static char _square_in(char file, char rank)
{
    char			c=0;
    char            square[] = "..";

    if (file < 'a' || file > 'h') {
        MAKE_SQUARE(file, rank, square)
        BAD_TYPE_IN("square", square);
    }
    if (rank < '1' ||  rank > '8') {
        MAKE_SQUARE(file, rank, square)
        BAD_TYPE_IN("square", square);
    }

    c = (file - 'a') + ( 8 * (rank - '1'));

    CH_DEBUG5("_square_in: file:%c rank:%c char:%i", file, rank, c);
#ifdef EXTRA_DEBUG
    CH_DEBUG5("_square_in char of file rank: %c%c:", CHAR_CFILE(c), CHAR_RANK(c));
    CH_DEBUG5("c file:%i c rank:%i", TO_FILE(c), TO_RANK(c));
    CH_DEBUG5("file - 'a':%i rank-'1':%i", file -'a', rank-'1');
#endif

    if (c < 0 || c > 63) {
        MAKE_SQUARE(file, rank, square);
        CH_ERROR("bad conversion for square %s" ,square);
    }
    return c;
}

static char *_square_out(char c, char *str)
{

    if (c < 0 || c > 63)
        BAD_TYPE_OUT("square", c);

    switch (c % 8) {
        case 0: str[0]='a'; break;
        case 1: str[0]='b'; break;
        case 2: str[0]='c'; break;
        case 3: str[0]='d'; break;
        case 4: str[0]='e'; break;
        case 5: str[0]='f'; break;
        case 6: str[0]='g'; break;
        case 7: str[0]='h'; break;

        default: CH_ERROR("bad switch statement %d", c);
    }

    switch (c / 8) {
        case 0: str[1]='1'; break;
        case 1: str[1]='2'; break;
        case 2: str[1]='3'; break;
        case 3: str[1]='4'; break;
        case 4: str[1]='5'; break;
        case 5: str[1]='6'; break;
        case 6: str[1]='7'; break;
        case 7: str[1]='8'; break;

        default: CH_ERROR("bad switch statement %d", c);
    }
    return str;
}


Datum
square_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);

    if (strlen(str) != 2)
        BAD_TYPE_IN("square", str); 

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
int_to_square(PG_FUNCTION_ARGS)
{
    char            c = (char)PG_GETARG_INT32(0);
    char			*result = (char *) palloc(3);

    if (c < 0 || c > 63)
        CH_ERROR("value must be in between 0 and 63");
    PG_RETURN_CHAR(c);

    _square_out(c, result);
    result[2] = '\0';
    PG_RETURN_CSTRING(result);
}

/********************************************************
* 		piece
********************************************************/

static char	_piece_out(char piece)
{

    char        val;
	switch (piece) {
        case 1: val='P'; break;
        case 2: val='p'; break;
        case 3: val='N'; break;
        case 4: val='n'; break;
        case 5: val='B'; break;
        case 6: val='b'; break;
        case 7: val='R'; break;
        case 8: val='r'; break;
        case 9: val='Q'; break;
        case 10: val='q'; break;
        case 11: val='K'; break;
        case 12: val='k'; break;
		default:
			BAD_TYPE_OUT("piece", piece);
			break;
	}
	return val;
}


Datum
piece_in(PG_FUNCTION_ARGS)
{
	char 			*str = PG_GETARG_CSTRING(0);
    char            val;

	if (strlen(str) != 1)
		BAD_TYPE_IN("piece", str);

	switch (str[0]) {
        case 'P': val=1; break;
        case 'p': val=2; break;
        case 'N': val=3; break;
        case 'n': val=4; break;
        case 'B': val=5; break;
        case 'b': val=6; break;
        case 'R': val=7; break;
        case 'r': val=8; break;
        case 'Q': val=9; break;
        case 'q': val=10; break;
        case 'K': val=11; break;
        case 'k': val=12; break;
		default:
			BAD_TYPE_IN("piece", str);
			break;
	}
	PG_RETURN_CHAR(val);
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
 * 		diagonal
 ********************************************************/

static char _diagonal_in(char square)
{
    char            d;

    switch (square+1) {
        case 1:
            d = -7; break;
        case 9: case 2:
            d = -6; break;
        case 17: case 10: case 3:
            d = -5; break;
        case 25: case 18: case 11: case 4:
            d = -4; break;
        case 33: case 26: case 19: case 12: case 5:
            d = -3; break;
        case 41: case 34: case 27: case 20: case 13: case 6:
            d = -2; break;
        case 49: case 42: case 35: case 28: case 21: case 14: case 7:
            d = -1; break;
        case 57: case 50: case 43: case 36: case 29: case 22: case 15: case 8:
            d = 0; break;
        case 58: case 51: case 44: case 37: case 30: case 23: case 16:
            d = 1; break;
        case 59: case 52: case 45: case 38: case 31: case 24:
            d = 2; break;
        case 60: case 53: case 46: case 39: case 32:
            d = 3; break;
        case 61: case 54: case 47: case 40:
            d = 4; break;
        case 62: case 55: case 48:
            d = 5; break;
        case 63: case 56:
            d = 6; break;
        case 64:
            d = 7; break;
        default:
            CH_ERROR("bad square %d for diagonal", square);
            break;
    }
    return d;
}

Datum
square_to_diagonal(PG_FUNCTION_ARGS)
{
    char 			s = PG_GETARG_CHAR(0);
    PG_RETURN_CHAR(_diagonal_in(s));
}

Datum
diagonal_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);
    if (strlen(str) != 2)
        BAD_TYPE_IN("diagonal", str); 

    PG_RETURN_CHAR(_diagonal_in(_square_in(str[0],str[1])));
}

Datum
diagonal_out(PG_FUNCTION_ARGS)
{
    char 			d = PG_GETARG_CHAR(0);
    char            *result = palloc(3);

    switch (d) {
        case -7: strcpy(result, "a1"); break;
        case -6: strcpy(result, "a2"); break;
        case -5: strcpy(result, "a3"); break;
        case -4: strcpy(result, "a4"); break;
        case -3: strcpy(result, "a5"); break;
        case -2: strcpy(result, "a6"); break;
        case -1: strcpy(result, "a7"); break;
        case  0: strcpy(result, "a8"); break;
        case  1: strcpy(result, "b8"); break;
        case  2: strcpy(result, "c8"); break;
        case  3: strcpy(result, "d8"); break;
        case  4: strcpy(result, "e8"); break;
        case  5: strcpy(result, "f8"); break;
        case  6: strcpy(result, "g8"); break;
        case  7: strcpy(result, "h8"); break;
    }
    result[3] = '\0';
    PG_RETURN_CSTRING(result);
}



/********************************************************
 * 		adiagonal
 ********************************************************/

static char _adiagonal_in(char square)
{
    char            d;

    switch (square+1) {
        case 57:
            d = -7; break;
        case 49: case 58:
            d = -6; break;
        case 41: case 50: case 59:
            d = -5; break;
        case 33: case 42: case 51: case 60:
            d = -4; break;
        case 25: case 34: case 43: case 52: case 61:
            d = -3; break;
        case 17: case 26: case 35: case 44: case 53: case 62:
            d = -2; break;
        case 9: case 18: case 27: case 36: case 45: case 54: case 63:
            d = -1; break;
        case 1: case 10: case 19: case 28: case 37: case 46: case 55: case 64:
            d = 0; break;
        case 2: case 11: case 20: case 29: case 38: case 47: case 56:
            d = 1; break;
        case 3: case 12: case 21: case 30: case 39: case 48:
            d = 2; break;
        case 4: case 13: case 22: case 31: case 40:
            d = 3; break;
        case 5: case 14: case 23: case 32:
            d = 4; break;
        case 6: case 15: case 24:
            d = 5; break;
        case 7: case 16:
            d = 6; break;
        case 8:
            d = 7; break;
        default:
            CH_ERROR("bad square %d for adiagonal", square);
            break;
    }
    return d;
}

Datum
square_to_adiagonal(PG_FUNCTION_ARGS)
{
    char 			s = PG_GETARG_CHAR(0);
    PG_RETURN_CHAR(_adiagonal_in(s));
}

    Datum
adiagonal_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);
    if (strlen(str) != 2)
        BAD_TYPE_IN("diagonal", str); 

    PG_RETURN_CHAR(_adiagonal_in(_square_in(str[0],str[1])));
}

Datum
adiagonal_out(PG_FUNCTION_ARGS)
{
    char 			d = PG_GETARG_CHAR(0);
    char            *result = palloc(3);

    switch (d) {
        case -7: strcpy(result, "a8"); break;
        case -6: strcpy(result, "a7"); break;
        case -5: strcpy(result, "a6"); break;
        case -4: strcpy(result, "a5"); break;
        case -3: strcpy(result, "a4"); break;
        case -2: strcpy(result, "a3"); break;
        case -1: strcpy(result, "a2"); break;
        case  0: strcpy(result, "a1"); break;
        case  1: strcpy(result, "b1"); break;
        case  2: strcpy(result, "c1"); break;
        case  3: strcpy(result, "d1"); break;
        case  4: strcpy(result, "e1"); break;
        case  5: strcpy(result, "f1"); break;
        case  6: strcpy(result, "g1"); break;
        case  7: strcpy(result, "h1"); break;
    }
    result[3] = '\0';
    PG_RETURN_CSTRING(result);
}

/********************************************************
 * 		fen
 ********************************************************/

/* 
 * This is a really nice compact way to store fen without Huffman compresion:
 * https://codegolf.stackexchange.com/questions/19397/smallest-chess-board-compression
 * second answer : 192 bits
 */
 
static int _board_compare(const Board *a, const Board *b) 
{
    if (a->pcount > b->pcount)
        return 1;
    else if (a->pcount < b->pcount)
        return -1;
    else
        return (memcmp(a, b, sizeof(a) + a->pcount));
}

Datum
board_cmp(PG_FUNCTION_ARGS)
{
    const Board     *a = (Board *) PG_GETARG_POINTER(0);
    const Board     *b = (Board *) PG_GETARG_POINTER(1);
    PG_RETURN_INT32(_board_compare(a, b));
}

Datum
board_eq(PG_FUNCTION_ARGS)
{
    const Board     *a = (Board *) PG_GETARG_POINTER(0);
    const Board     *b = (Board *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(_board_compare(a, b) == 0);
}

Datum
board_ne(PG_FUNCTION_ARGS)
{
    const Board     *a = (Board *) PG_GETARG_POINTER(0);
    const Board     *b = (Board *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(_board_compare(a, b) != 0);
}

Datum
board_lt(PG_FUNCTION_ARGS)
{
    const Board     *a = (Board *) PG_GETARG_POINTER(0);
    const Board     *b = (Board *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(_board_compare(a, b) < 0);
}

Datum
board_gt(PG_FUNCTION_ARGS)
{
    const Board     *a = (Board *) PG_GETARG_POINTER(0);
    const Board     *b = (Board *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(_board_compare(a, b) > 0);
}

Datum
board_le(PG_FUNCTION_ARGS)
{
    const Board     *a = (Board *) PG_GETARG_POINTER(0);
    const Board     *b = (Board *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(_board_compare(a, b) <= 0);
}

Datum
board_ge(PG_FUNCTION_ARGS)
{
    const Board     *a = (Board *) PG_GETARG_POINTER(0);
    const Board     *b = (Board *) PG_GETARG_POINTER(1);
    PG_RETURN_BOOL(_board_compare(a, b) >= 0);
}

Datum
board_hash(PG_FUNCTION_ARGS)
{

	const Board     *b = (Board *) PG_GETARG_POINTER(0);
	char            str[MAX_FEN];

	_board_fen(b, str);
	PG_RETURN_INT64(_sdbm_hash(str));
}



Datum
board_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);
    Board           *result;
    unsigned char   c, p='\0', pieces[MAX_PIECES];
    int64           bitboard=0;
    int             i=0, j=BOARD_SIZE-1, k=0, s;
    bool            done=false, wk=false, wq=false, bk=false, bq=false;
    char            enpassant=-1, whitesgo=-1;
    size_t          s1, s2;

    if (strlen(str) > MAX_FEN)
        CH_ERROR("fen string too long");
    memset(pieces, 0, MAX_PIECES);

    // fast forward to move side
    while (str[i] != '\0') {
        c = str[i++];
        if (c == ' ') break;
    }
    switch (str[i++]) {
        case 'w': whitesgo=1; break;
        case 'b': whitesgo=0; break;
        default: CH_ERROR("bad move side in fen"); break;
    }
    i++;
    while (str[i] != '\0' && str[i] != ' ') {
        switch (str[i++]) {
            case 'K': wk=true; break;
            case 'Q': wq=true; break;
            case 'k': bk=true; break;
            case 'q': bq=true; break;
            case '-': break;
            default: CH_ERROR("bad castle availability in fen"); break;
        }
    }
    c = str[++i];
    if (c >= 'a' && c <= 'h') {
        p = str[++i];
        if (p != '3' && p != '6')
            CH_ERROR("bad enpassant rank in fen '%c'", p);
        enpassant = _square_in(c, p);

        CH_DEBUG5("_fen_in: enpessant: %c%c:", CHAR_CFILE(enpassant), CHAR_RANK(enpassant));

        // enpassant is set on the capture not on the pawn
        // so we have have to go up a rank to find the pawn
        enpassant += (p=='3' ? 8 : -8);

    } else if (c=='-') {
    } else {
        CH_DEBUG5("fen:'%s':", str);
        CH_ERROR("bad enpassant in fen: '%c%i':", c, p);
    }

    i = 0;
    while (str[i] != '\0') {

        // i indexes differently than square type (for enpassant)
        s = TO_SQUARE_IDX(j);

        switch (c=str[i]) {
                case 'p': case 'n': case 'b': case 'r': case 'q': case 'k':
                case 'P': case 'N': case 'B': case 'R': case 'Q': case 'K':

                    CH_DEBUG5("_fen_in: :square: %c%c: str[i]:%c", CHAR_CFILE(s), CHAR_RANK(s), str[i]);
                    SET_BOARD(bitboard, j);
                    if (s==enpassant && str[i] != 'p' && str[i] != 'P') {
                        CH_ERROR("no pawn found for enpassant at %c%c", CHAR_CFILE(s), CHAR_RANK(s));
                    }
                    switch(str[i]) {
                        case 'p': p = BLACK_PAWN; break;
                        case 'n': p = BLACK_KNIGHT; break;
                        case 'b': p = BLACK_BISHOP; break;
                        case 'r': p = BLACK_ROOK; break;
                        case 'q': p = BLACK_QUEEN; break;
                        case 'k': p = BLACK_KING; break;

                        case 'P': p = WHITE_PAWN; break;
                        case 'N': p = WHITE_KNIGHT; break;
                        case 'B': p = WHITE_BISHOP; break;
                        case 'R': p = WHITE_ROOK; break;
                        case 'Q': p = WHITE_QUEEN; break;
                        case 'K': p = WHITE_KING; break;
                    }
                    SET_PIECE(pieces, k, p);
                    k++;
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
                          CH_ERROR("unkdown character in fen '%c'", c);
                          break;
        }
        if (k>MAX_PIECES)
            CH_ERROR("too many pieces in fen");
        i++;
        if (done) 
            break;
        if (j<-1)
            CH_ERROR("FEN board is too long");
    }

    if (j>-1)
        CH_ERROR("FEN board is too short");


    s1 = k/2 + k%2 ; // pieces size
    s2 = sizeof(Board);
    //CH_NOTICE("sizeof board %li sizeof pieces:%li", s2, s1);
    result = (Board*)palloc(s2 + s1);
    memset(result, 0, s1+s2);
    SET_VARSIZE(result, s1+s2);

#ifdef EXTRA_DEBUG
	debug_bitboard(bitboard);
	strcpy(result->orig_fen, str);
#endif

    result->board = bitboard;
    result->wk = wk;
    result->wq = wq;
    result->bk = bk;
    result->bq = bq;
    result->enpassant = enpassant;
    result->whitesgo = whitesgo;
    result->pcount = k;
    memcpy(result->pieces, pieces, s1);

    /*
    for (i=0; i<k; i++)
        CH_DEBUG5("piece %i:0x%08X", i, pieces[i]);
    */

    PG_RETURN_POINTER(result);
}


static int _board_fen(const Board * b, char * str)
{
    int             i;
    unsigned char   j=0, k=0, empties=0, s, piece;

#ifdef EXTRA_DEBUG
    debug_bitboard(b->board);
    CH_DEBUG5("orig fen: %s", b->orig_fen);
	CH_DEBUG5("piece count: %i", b->pcount);
#endif
    for (i=BOARD_SIZE-1; i>=0; i--) {
        if (j >= MAX_FEN) {
#ifdef EXTRA_DEBUG
            CH_ERROR("internal error: fen is too long; original fen:'%s'", b->orig_fen);
#else 
            CH_ERROR("internal error: fen is too long");
#endif
}
        if (k > b->pcount) {
#ifdef EXTRA_DEBUG
    CH_ERROR("internal error: too many pieces; original fen:'%s'", b->orig_fen);
#else 
    CH_ERROR("internal error: too many pieces");
#endif
        }
        // need for square type
        s = TO_SQUARE_IDX(i);

        // if its an empty square
        if (!CHECK_BIT(b->board, i)) {
            empties += 1;
		// else there is apiece
        } else {
            // if we have some empties then empty them
            if (empties) 
                str[j++] = empties + '0';

            empties = 0;
            piece = GET_PIECE(b->pieces, k);
            k++;

            switch(piece) {
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
                    // should not get here :)
                    CH_ERROR("internal error: unknown piece type at piece %i in board: %i", k, piece);
                    break;
            }
			CH_DEBUG5("_fen_out: i:%i :square: %c%c piece:%c",i, CHAR_CFILE(s), CHAR_RANK(s), str[j-1]);
        }
        // if were at the end or a row add '/' and perhaps and empty number
        if (i%8==0) { 
            if (empties) 
                str[j++] = empties + '0';
            if (i) 
                str[j++] = '/';
            empties = 0;
        }
    }
    str[j++] = ' ';
    str[j++] = b->whitesgo ? 'w' : 'b';

    str[j++] = ' ';
    if (b->wk + b->wq + b->bk + b->bq > 0) {
        if (b->wk) str[j++] = 'K';
        if (b->wq) str[j++] = 'Q';
        if (b->bk) str[j++] = 'k';
        if (b->bq) str[j++] = 'q';
    } else
        str[j++] = '-';

    str[j++] = ' ';
    CH_DEBUG5("enp: %i", b->enpassant);
    if (b->enpassant > -1) {
        str[j++] = CHAR_CFILE(b->enpassant);
        str[j++] = CHAR_RANK(b->enpassant);
    } else
        str[j++] = '-';

    str[j++] = '\0';
    return j;
}

Datum
board_out(PG_FUNCTION_ARGS)
{

    const Board     *b = (Board *) PG_GETARG_POINTER(0);
    char            str[MAX_FEN], n;
    char            *result;

    n = _board_fen(b, str);
    result = (char *)palloc(n);
    memcpy(result, str, n);
    PG_RETURN_CSTRING(result);
}

Datum
piece_count(PG_FUNCTION_ARGS)
{
    const Board     *b = (Board *) PG_GETARG_POINTER(0);
    PG_RETURN_CSTRING(b->pcount);
}

Datum
board_go(PG_FUNCTION_ARGS)
{
    const Board     *b = (Board *) PG_GETARG_POINTER(0);
    char            *str = palloc(2);

    str[0] = b->whitesgo ? 'w' : 'b';
    str[1] = '\0';
    PG_RETURN_CSTRING(str);
}
