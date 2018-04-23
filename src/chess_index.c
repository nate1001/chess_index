
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


const int	MAX_FEN = 100;
const int	MAX_PIECES = 32;
const int	MAX_MOVES = 192;
const int	MAX_SAN = 10;

// postgresql type names
const char TYPE_PIECESQUARE[]  = "piecesquare";
const char TYPE_SQUARE[] = "square";
const char TYPE_PIECE[] = "piece";


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

/*
PG_FUNCTION_INFO_V1(piecesquare_in);
PG_FUNCTION_INFO_V1(piecesquare_out);
PG_FUNCTION_INFO_V1(piecesquare_square);
PG_FUNCTION_INFO_V1(piecesquare_piece);
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

