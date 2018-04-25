
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

//#define EXTRA_DEBUG 1

PG_FUNCTION_INFO_V1(char_to_int);

PG_FUNCTION_INFO_V1(chess_cmp);
PG_FUNCTION_INFO_V1(chess_cmp_eq);
PG_FUNCTION_INFO_V1(chess_cmp_neq);
PG_FUNCTION_INFO_V1(chess_cmp_gt);
PG_FUNCTION_INFO_V1(chess_cmp_lt);
PG_FUNCTION_INFO_V1(chess_cmp_lteq);
PG_FUNCTION_INFO_V1(chess_cmp_gteq);

PG_FUNCTION_INFO_V1(square_in);
PG_FUNCTION_INFO_V1(square_out);
PG_FUNCTION_INFO_V1(int_to_square);

PG_FUNCTION_INFO_V1(piece_in);
PG_FUNCTION_INFO_V1(piece_out);

PG_FUNCTION_INFO_V1(fen_in);
PG_FUNCTION_INFO_V1(fen_out);

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

static char *_square_out(char c, char *str);

#define CHECK_BIT(board, k) ((1ul << k) & board)
#define GET_PIECE(pieces, k) k%2 ? pieces[k/2] & 0x0f : (pieces[k/2] & 0xf0) >> 4
#define SET_PIECE(pieces, k, v) pieces[k/2] = k%2 ? ( (pieces[k/2] & 0xF0) | (v & 0xF)) : ((pieces[k/2] & 0x0F) | (v & 0xF) << 4)
#define TO_SQUARE_IDX(i)  (i/8)*8 + (8 - i%8) - 1;
#define CHAR_CFILE(s) 'a' + _square_to_cfile(s)
#define CHAR_RANK(s) '1' + _square_to_rank(s)
#define MAKE_SQUARE(file, rank, str) {str[0]=file; str[1]=rank;}


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

/*
 * max size -> 28: 32 pieces of 4 bit nibbles = 16 bytes + 8 byte bitboard + 4 byte length (required by pg)
 * min size -> 13: 3 pieces of 4 bit nibbles = 1 byte + 8 byte bitboard + 4 byte length (required by pg)
 */
typedef struct {
    int32           vl_len;
    int64           board;
    unsigned char   pieces[FLEXIBLE_ARRAY_MEMBER];
} board;

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

static char _square_to_cfile(char s) 
{
    return s%8;
}

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
    PG_RETURN_CHAR(_square_to_cfile(s));
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

static char _square_to_rank(char s) 
{
    return s/8;
}

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
    PG_RETURN_CHAR(_square_to_rank(s));
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
    CH_DEBUG5("c file:%i c rank:%i", _square_to_cfile(c), _square_to_rank(c));
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

    Datum
fen_in(PG_FUNCTION_ARGS)
{
    char 			*str = PG_GETARG_CSTRING(0);
    board           *result;
    unsigned char   c, p='\0', pieces[MAX_PIECES];
    int64           bitboard=0;
    int             i=0, j=BOARD_SIZE-1, k=0, s;
    bool            done=false, whitesgo=false, wk=false, wq=false, bk=false, bq=false, epfound=false;
    char            enpassant=-1;
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
        case 'w': whitesgo=true; break;
        case 'b': whitesgo=false; break;
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
    if (c >= 'a' && c <= 'f') {
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
        CH_ERROR("bad enpassant in fen '%c'", enpassant);
    }

    i = 0;
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
                // i indexes differently than square type (for enpassant)
                s = TO_SQUARE_IDX(j);
                CH_DEBUG5("_fen_in: :square: %c%c: str[i]:%c", CHAR_CFILE(s), CHAR_RANK(s), str[i]);
                bitboard |= (1Ul << (j--));
                if (s==enpassant) {
                    epfound = true;
                    if (str[i] == 'p')
                        p = BLACK_SPECIAL;
                    else if (str[i] == 'P')
                        p = WHITE_SPECIAL;
                    else
                        CH_ERROR("no pawn found for enpassant at %c%c", CHAR_CFILE(s), CHAR_RANK(s));
                } else {
                // second switch for pieces
                    switch (c=str[i]) {
                        case 'p': p = BLACK_PAWN; break;
                        case 'n': p = BLACK_KNIGHT; break;
                        case 'b': p = BLACK_BISHOP; break;
                        case 'r': p = bk && s==63 ? BLACK_SPECIAL : bq && s==56 ? BLACK_SPECIAL : BLACK_ROOK; break;
                        case 'q': p = BLACK_QUEEN; break;
                        case 'K': p = whitesgo ? WHITE_KING_MOVE : WHITE_KING; break;

                        case 'P': p = WHITE_PAWN; break;
                        case 'N': p = WHITE_KNIGHT; break;
                        case 'B': p = WHITE_BISHOP; break;
                        case 'R': p = wk && s==7 ? WHITE_SPECIAL : wq && s==0 ? WHITE_SPECIAL : WHITE_ROOK; break;
                        case 'Q': p = WHITE_QUEEN; break;
                        case 'k': p = whitesgo ? BLACK_KING : BLACK_KING_MOVE; break;
                    }
                }
                SET_PIECE(pieces, k, p);
                k++;
                if (k>MAX_PIECES)
                    CH_ERROR("too many pieces in fen");
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
                BAD_TYPE_IN("fen", str);
                break;
        }
        i++;
        if (done) 
            break;
        if (j<-1)
            CH_ERROR("FEN board is too long");
    }

    if (j>-1)
        CH_ERROR("FEN board is too short");

#ifdef EXTRA_DEBUG
    debug_bitboard(bitboard);
#endif

    if (enpassant != -1 && !epfound)
        CH_ERROR("en passant set for %c%c but no pawn was found", CHAR_CFILE(enpassant), CHAR_RANK(enpassant));

    s1 = k/2 + k%2 ; // pieces size
    //  bitboard            length
    s2 = sizeof(int64) +  sizeof(int32);
    result = (board*)palloc(s1+s2);
    memset(result, 0, s1+s2);
    SET_VARSIZE(result, s1+s2);

    result->board = bitboard;
    memcpy(result->pieces, pieces, s1);

    PG_RETURN_POINTER(result);
}

Datum
fen_out(PG_FUNCTION_ARGS)
{

    //_int_gist.c:    GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);

    const board     *b = (board *) PG_GETARG_POINTER(0);
    char            *str = palloc(MAX_FEN);
    char            *result;
    unsigned char   piece;
    int             i, j=0, k=0,empties=0, s;
    bool            wk=false, wq=false, bk=false, bq=false;
    char            enpassant=-1, whitesgo=-1;


#ifdef EXTRA_DEBUG
    debug_bitboard(b->board);
#endif

    for (i=BOARD_SIZE-1; i>=0; i--) {

        // need for square type
        s = TO_SQUARE_IDX(i);

        CH_DEBUG5("_fen_out: :square: %c%c", CHAR_CFILE(s), CHAR_RANK(s));

        // if its an empty square
        if (!CHECK_BIT(b->board, i)) {
            empties += 1;
            // else there is apiece
        } else {
            // if we have some empties empty them
            if (empties) str[j++] = empties + '0';
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
                case BLACK_KING_MOVE: str[j++] = 'k'; whitesgo=0; break;
                case WHITE_PAWN:    str[j++] = 'P'; break;
                case WHITE_KNIGHT:  str[j++] = 'N'; break;
                case WHITE_BISHOP:  str[j++] = 'B'; break;
                case WHITE_ROOK:    str[j++] = 'R'; break;
                case WHITE_QUEEN:   str[j++] = 'Q'; break;
                case WHITE_KING:    str[j++] = 'K'; break;
                case WHITE_KING_MOVE: str[j++] = 'K'; whitesgo=1; break;

                case WHITE_SPECIAL:
                          // if its a castle special
                          if (_square_to_rank(s)==0) {
                                str[j++] = 'R';
                                switch (_square_to_cfile(s)) {
                                    case 0: wq=true; break;
                                    case 7: wk=true; break;
                                    default: 
                                        CH_ERROR("bad file for white castle: %c", _square_to_cfile(s));
                                        break;
                                }
                          // if its a en passant special
                          } else if (_square_to_rank(s)==3) {
                              enpassant = s;
                              str[j++] = 'P';
                          } else {
                              CH_ERROR("bad internal 'white special' at %c%c", CHAR_CFILE(s), CHAR_RANK(s));
                          }
                          break;
                case BLACK_SPECIAL:
                          // if its a castle special
                          if (_square_to_rank(s)==7) {
                              str[j++] = 'r';
                              switch (_square_to_cfile(s)) {
                                  case 0: bq=true; break;
                                  case 7: bk=true; break;
                                  default: 
                                          CH_ERROR("bad file for black castle: %c", _square_to_cfile(s));
                                          break;
                              }
                              // if its a en passant special
                          } else if (_square_to_rank(s)==4) {
                              enpassant = s;
                              str[j++] = 'p';
                          } else {
                              CH_ERROR("bad internal 'black special' at %c%c", CHAR_CFILE(s), CHAR_RANK(s));
                          }
                          break;

                default:
                          // should not get here :)
                          BAD_TYPE_OUT("fen", piece);
                          break;
            }
        }
        // if were at the end or a row add '/' and perhaps and empty number
        if (i%8==0) { 
            if (empties) str[j++] = empties + '0';
            if (i) str[j++] = '/';
            empties = 0;
        }
    }
    str[j++] = ' ';
    if (whitesgo==0)
        str[j++] = 'b';
    else if (whitesgo==1)
        str[j++] = 'w';
    else
        CH_ERROR("side to move not found");

    str[j++] = ' ';
    if (wk + wq + bk + bq > 0) {
        if (wk) str[j++] = 'K';
        if (wq) str[j++] = 'Q';
        if (bk) str[j++] = 'k';
        if (bq) str[j++] = 'q';
    } else
        str[j++] = '-';

    str[j++] = ' ';
    if (enpassant > -1) {
        str[j++] = CHAR_CFILE(enpassant);
        str[j++] = CHAR_RANK(enpassant);
    } else
        str[j++] = '-';

    str[j++] = '\0';
    result = (char *)palloc(j);
    memcpy(result, str, j);

    PG_RETURN_CSTRING(result);
}

