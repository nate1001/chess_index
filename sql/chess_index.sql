
-- complain if script is sourced in psql, rather than via CREATE EXTENSION

\echo Use "CREATE EXTENSION chess_index" to load this file. \quit


CREATE FUNCTION square_in(cstring)
RETURNS square
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION square_out(square)
RETURNS cstring
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE square(
  INPUT          = square_in,
  OUTPUT         = square_out,
  LIKE           = char,
	INTERNALLENGTH = 1,     -- use 4 bytes to store data
	ALIGNMENT      = char,  -- align to 4 bytes
	STORAGE        = PLAIN, -- always store data inline uncompressed (not toasted)
	PASSEDBYVALUE           -- pass data by value rather than by reference

);

CREATE FUNCTION square_to_int(square)
RETURNS int4
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (square AS int4) WITH FUNCTION square_to_int(square);

CREATE FUNCTION int_to_square(int4)
RETURNS square
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (int4 AS square) WITH FUNCTION int_to_square(int4);

CREATE FUNCTION square_eq(square, square)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'chareq';
CREATE FUNCTION square_ne(square, square)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'charne';
CREATE FUNCTION square_lt(square, square)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'charlt';
CREATE FUNCTION square_le(square, square)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'charle';
CREATE FUNCTION square_gt(square, square)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'chargt';
CREATE FUNCTION square_ge(square, square)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'charge';
CREATE FUNCTION square_cmp(square, square)
RETURNS integer LANGUAGE internal IMMUTABLE AS 'btcharcmp';
CREATE FUNCTION hash_square(square)
RETURNS integer LANGUAGE internal IMMUTABLE AS 'hashchar';

CREATE OPERATOR = (
  LEFTARG = square,
  RIGHTARG = square,
  PROCEDURE = square_eq,
  COMMUTATOR = '=',
  NEGATOR = '<>',
  RESTRICT = eqsel,
  JOIN = eqjoinsel,
  HASHES, MERGES
);

CREATE OPERATOR <> (
  LEFTARG = square,
  RIGHTARG = square,
  PROCEDURE = square_ne,
  COMMUTATOR = '<>',
  NEGATOR = '=',
  RESTRICT = neqsel,
  JOIN = neqjoinsel
);

CREATE OPERATOR < (
  LEFTARG = square,
  RIGHTARG = square,
  PROCEDURE = square_lt,
  COMMUTATOR = > ,
  NEGATOR = >= ,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
  LEFTARG = square,
  RIGHTARG = square,
  PROCEDURE = square_le,
  COMMUTATOR = >= ,
  NEGATOR = > ,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
  LEFTARG = square,
  RIGHTARG = square,
  PROCEDURE = square_gt,
  COMMUTATOR = < ,
  NEGATOR = <= ,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
  LEFTARG = square,
  RIGHTARG = square,
  PROCEDURE = square_ge,
  COMMUTATOR = <= ,
  NEGATOR = < ,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE OPERATOR CLASS btree_square_ops
DEFAULT FOR TYPE square USING btree
AS
        OPERATOR        1       <  ,
        OPERATOR        2       <= ,
        OPERATOR        3       =  ,
        OPERATOR        4       >= ,
        OPERATOR        5       >  ,
        FUNCTION        1       square_cmp(square, square);

CREATE OPERATOR CLASS hash_square_ops
    DEFAULT FOR TYPE square USING hash AS
        OPERATOR        1       = ,
        FUNCTION        1       hash_square(square);


/****************************************************************************
-- piece
****************************************************************************/

CREATE FUNCTION piece_in(cstring)
RETURNS piece
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION piece_out(piece)
RETURNS cstring
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE piece(
  INPUT          = piece_in,
  OUTPUT         = piece_out,
  LIKE           = char,
	INTERNALLENGTH = 1,     -- use 4 bytes to store data
	ALIGNMENT      = char,  -- align to 4 bytes
	STORAGE        = PLAIN, -- always store data inline uncompressed (not toasted)
	PASSEDBYVALUE           -- pass data by value rather than by reference
);

CREATE FUNCTION piece_eq(piece, piece)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'chareq';
CREATE FUNCTION piece_ne(piece, piece)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'charne';

CREATE OPERATOR = (
    LEFTARG = piece,
    RIGHTARG = piece,
    PROCEDURE = piece_eq,
    COMMUTATOR = '=',
    NEGATOR = '<>',
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES, MERGES
);

CREATE OPERATOR <> (
    LEFTARG = piece,
    RIGHTARG = piece,
    PROCEDURE = piece_ne,
    COMMUTATOR = '<>',
    NEGATOR = '=',
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);


/****************************************************************************
-- board
 ****************************************************************************/

CREATE FUNCTION board_in(cstring)
RETURNS board
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION board_out(board)
RETURNS cstring
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE board(
    INPUT          = board_in,
    OUTPUT         = board_out,
    ALIGNMENT      = int4,  -- align to 4 bytes
    STORAGE        = PLAIN  -- always store data inline uncompressed (not toasted)
);


