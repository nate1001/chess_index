
-- complain if script is sourced in psql, rather than via CREATE EXTENSION

\echo Use "CREATE EXTENSION chess_index" to load this file. \quit

-- set client_min_messages=DEBUG5;

/****************************************************************************
-- side : white or black
 ****************************************************************************/
/*{{{*/
CREATE FUNCTION side_in(cstring)
RETURNS side AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION side_out(side)
RETURNS cstring AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE side(
    INPUT          = side_in,
    OUTPUT         = side_out,

    LIKE           = char,
    INTERNALLENGTH = 1,     
    ALIGNMENT      = char,
    STORAGE        = PLAIN,
    PASSEDBYVALUE         
);

CREATE FUNCTION side_eq(side, side)
RETURNS bool LANGUAGE internal IMMUTABLE as 'chareq';
CREATE FUNCTION side_ne(side, side)
RETURNS bool LANGUAGE internal IMMUTABLE as 'charne';
CREATE FUNCTION "not"(side)
RETURNS side AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR = (
    LEFTARG = side,
    RIGHTARG = side,
    PROCEDURE = side_eq,
    COMMUTATOR = '=',
    NEGATOR = '<>',
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES, MERGES
);

CREATE OPERATOR <> (
    LEFTARG = side,
    RIGHTARG = side,
    PROCEDURE = side_ne,
    COMMUTATOR = '<>',
    NEGATOR = '=',
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);
/*}}}*//*}}}*/
/****************************************************************************
-- pindex : piece index
 ****************************************************************************/
/*{{{*/
CREATE FUNCTION pindex_in(cstring)
RETURNS pindex AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION pindex_out(pindex)
RETURNS cstring AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE pindex(
     INPUT          = pindex_in
    ,OUTPUT         = pindex_out
    ,LIKE           = int2
);

CREATE FUNCTION pindex_to_int32(pindex)
RETURNS int4 AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (pindex as int4) WITH FUNCTION pindex_to_int32;

CREATE FUNCTION pindex_eq(pindex, pindex)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'int2eq';
CREATE FUNCTION pindex_ne(pindex, pindex)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'int2ne';
CREATE FUNCTION pindex_lt(pindex, pindex)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'int2lt';
CREATE FUNCTION pindex_le(pindex, pindex)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'int2le';
CREATE FUNCTION pindex_gt(pindex, pindex)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'int2gt';
CREATE FUNCTION pindex_ge(pindex, pindex)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'int2ge';
CREATE FUNCTION pindex_cmp(pindex, pindex)
RETURNS integer LANGUAGE internal IMMUTABLE AS 'btint2cmp';
CREATE FUNCTION hash_pindex(pindex)
RETURNS integer LANGUAGE internal IMMUTABLE AS 'hashint2';

CREATE OPERATOR = (
    LEFTARG = pindex,
    RIGHTARG = pindex,
    PROCEDURE = pindex_eq,
    COMMUTATOR = '=',
    NEGATOR = '<>',
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES, MERGES
);

CREATE OPERATOR <> (
    LEFTARG = pindex,
    RIGHTARG = pindex,
    PROCEDURE = pindex_ne,
    COMMUTATOR = '<>',
    NEGATOR = '=',
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

CREATE OPERATOR < (
  LEFTARG = pindex,
  RIGHTARG = pindex,
  PROCEDURE = pindex_lt,
  COMMUTATOR = > ,
  NEGATOR = >= ,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
  LEFTARG = pindex,
  RIGHTARG = pindex,
  PROCEDURE = pindex_le,
  COMMUTATOR = >= ,
  NEGATOR = > ,
  RESTRICT = scalarltsel,
  JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
  LEFTARG = pindex,
  RIGHTARG = pindex,
  PROCEDURE = pindex_gt,
  COMMUTATOR = < ,
  NEGATOR = <= ,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
  LEFTARG = pindex,
  RIGHTARG = pindex,
  PROCEDURE = pindex_ge,
  COMMUTATOR = <= ,
  NEGATOR = < ,
  RESTRICT = scalargtsel,
  JOIN = scalargtjoinsel
);

CREATE OPERATOR CLASS btree_pindex_ops
DEFAULT FOR TYPE pindex USING btree
AS
        OPERATOR        1       <  ,
        OPERATOR        2       <= ,
        OPERATOR        3       =  ,
        OPERATOR        4       >= ,
        OPERATOR        5       >  ,
        FUNCTION        1       pindex_cmp(pindex, pindex);

CREATE OPERATOR CLASS hash_pindex_ops
DEFAULT FOR TYPE pindex USING hash AS
OPERATOR        1       = ,
FUNCTION        1       hash_pindex(pindex);

/*}}}*/
/****************************************************************************
-- square:
 ****************************************************************************/
/*{{{*/
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
	INTERNALLENGTH = 1, 
	ALIGNMENT      = char,
	STORAGE        = PLAIN,
	PASSEDBYVALUE         
);

CREATE CAST ("char" AS square) WITHOUT FUNCTION;
CREATE CAST (square AS "char") WITHOUT FUNCTION;
CREATE FUNCTION int_to_square(int) RETURNS square AS $$ select $1::"char"::square $$ LANGUAGE SQL IMMUTABLE STRICT;
CREATE FUNCTION square_to_int(square) RETURNS int AS $$ select $1::"char"::int $$ LANGUAGE SQL IMMUTABLE STRICT;
CREATE CAST (int AS square) WITH FUNCTION int_to_square(int);
CREATE CAST (square AS int) WITH FUNCTION square_to_int(square);

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

/*}}}*/
/****************************************************************************
-- piece
****************************************************************************/
/*{{{*/
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
    INTERNALLENGTH = 1,    
	ALIGNMENT      = char, 
	STORAGE        = PLAIN,
	PASSEDBYVALUE         
);

CREATE FUNCTION piece_eq(piece, piece)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'chareq';
CREATE FUNCTION piece_ne(piece, piece)
RETURNS boolean LANGUAGE internal IMMUTABLE as 'charne';
CREATE FUNCTION hash_square(piece)
RETURNS integer LANGUAGE internal IMMUTABLE AS 'hashchar';

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

CREATE OPERATOR CLASS hash_piece_ops
DEFAULT FOR TYPE piece USING hash AS
OPERATOR        1       = ,
FUNCTION        1       hash_square(piece);

/*}}}*/
/****************************************************************************
-- piecesquare
****************************************************************************/
CREATE FUNCTION piecesquare_in(cstring)/*{{{*/
RETURNS piecesquare AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION piecesquare_out(piecesquare)
RETURNS cstring AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE piecesquare(
     INPUT          = piecesquare_in
    ,OUTPUT         = piecesquare_out
    ,STORAGE        = PLAIN
    ,LIKE           = int2
);

CREATE FUNCTION piecesquare_square(piecesquare)
RETURNS square AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (piecesquare as square) WITH FUNCTION piecesquare_square;

CREATE FUNCTION piecesquare_piece(piecesquare)
RETURNS piece AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (piecesquare as piece) WITH FUNCTION piecesquare_piece;

/*}}}*/
/****************************************************************************
-- board: displays as fen, holds position
 ****************************************************************************/
/*{{{*/
CREATE FUNCTION board_in(cstring)
RETURNS board AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION board_out(board)
RETURNS cstring AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE board(
    INPUT          = board_in,
    OUTPUT         = board_out,
    STORAGE        = PLAIN
);

CREATE FUNCTION pcount(board)
RETURNS int AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION side(board)
RETURNS side AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION pieces(board, side)
RETURNS pindex AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION board_eq(board, board)
RETURNS boolean AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION board_ne(board, board)
RETURNS boolean AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION board_lt(board, board)
RETURNS boolean AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION board_le(board, board)
RETURNS boolean AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION board_gt(board, board)
RETURNS boolean AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION board_ge(board, board)
RETURNS boolean AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION board_cmp(board, board)
RETURNS int AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION board_hash(board)
RETURNS int AS '$libdir/chess_index' LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR = (
    LEFTARG = board,
    RIGHTARG = board,
    PROCEDURE = board_eq,
    COMMUTATOR = '=',
    NEGATOR = '<>',
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES, MERGES
);

CREATE OPERATOR <> (
    LEFTARG = board,
    RIGHTARG = board,
    PROCEDURE = board_ne,
    COMMUTATOR = '<>',
    NEGATOR = '=',
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = board,
    RIGHTARG = board,
    PROCEDURE = board_lt,
    COMMUTATOR = > ,
    NEGATOR = >= ,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = board,
    RIGHTARG = board,
    PROCEDURE = board_le,
    COMMUTATOR = >= ,
    NEGATOR = > ,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = board,
    RIGHTARG = board,
    PROCEDURE = board_gt,
    COMMUTATOR = < ,
    NEGATOR = <= ,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = board,
    RIGHTARG = board,
    PROCEDURE = board_ge,
    COMMUTATOR = <= ,
    NEGATOR = < ,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR CLASS btree_board_ops
DEFAULT FOR TYPE board USING btree
AS
OPERATOR        1       <  ,
OPERATOR        2       <= ,
OPERATOR        3       =  ,
OPERATOR        4       >= ,
OPERATOR        5       >  ,
FUNCTION        1       board_cmp(board, board);

CREATE OPERATOR CLASS hash_board_ops
DEFAULT FOR TYPE board USING hash AS
OPERATOR        1       = ,
FUNCTION        1       board_hash(board);

/*}}}*/
/****************************************************************************
-- file
 ****************************************************************************/
/*{{{*/
CREATE FUNCTION cfile_in(cstring)
RETURNS cfile 
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION cfile_out(cfile)
RETURNS cstring
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE cfile(
    INPUT          = cfile_in,
    OUTPUT         = cfile_out,

    INTERNALLENGTH = 1,     
    ALIGNMENT      = char, 
    STORAGE        = PLAIN, -- always store data inline uncompressed (not toasted)
    PASSEDBYVALUE           -- pass data by value rather than by reference
);


CREATE FUNCTION square_to_cfile(square)
RETURNS cfile
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (square AS cfile) WITH FUNCTION square_to_cfile(square);

CREATE FUNCTION char_to_int(cfile)
RETURNS int4
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (cfile AS int4) WITH FUNCTION char_to_int(cfile);

/*}}}*/
/****************************************************************************
-- rank
 ****************************************************************************/
/*{{{*/
CREATE FUNCTION rank_in(cstring)
RETURNS rank
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION rank_out(rank)
RETURNS cstring
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE rank(
    INPUT          = rank_in,
    OUTPUT         = rank_out,

    INTERNALLENGTH = 1,     -- use 4 bytes to store data
    ALIGNMENT      = char,  -- align to 4 bytes
    STORAGE        = PLAIN, -- always store data inline uncompressed (not toasted)
    PASSEDBYVALUE           -- pass data by value rather than by reference
);


CREATE FUNCTION square_to_rank(square)
RETURNS rank
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (square AS rank) WITH FUNCTION square_to_rank(square);

CREATE FUNCTION char_to_int(rank)
RETURNS int4
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (rank AS int4) WITH FUNCTION char_to_int(rank);
/*}}}*/
/****************************************************************************
-- diagonal
 ****************************************************************************/
/*{{{*/
CREATE FUNCTION diagonal_in(cstring)
RETURNS diagonal
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION diagonal_out(diagonal)
RETURNS cstring
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE diagonal(
    INPUT          = diagonal_in,
    OUTPUT         = diagonal_out,

    INTERNALLENGTH = 1,     -- use 4 bytes to store data
    ALIGNMENT      = char,  -- align to 4 bytes
    STORAGE        = PLAIN, -- always store data inline uncompressed (not toasted)
    PASSEDBYVALUE           -- pass data by value rather than by reference
);


CREATE FUNCTION square_to_diagonal(square)
RETURNS diagonal
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (square AS diagonal) WITH FUNCTION square_to_diagonal(square);

CREATE FUNCTION char_to_int(diagonal)
RETURNS int4
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;
CREATE CAST (diagonal AS int4) WITH FUNCTION char_to_int(diagonal);
/*}}}*/

/****************************************************************************
-- sql functions
 ****************************************************************************/
/*{{{*/
CREATE OR REPLACE FUNCTION pretty(text, uni bool default false, fen bool default true)
RETURNS text AS $$
    select replace(replace(replace(replace(replace(replace(replace(replace(
            translate
            (
                 split_part($1, ' ', 1)
                , case when $2 then '/kqrbnpKQRBNP' else '/' end
                , case when $2
                    then E'\n' || U&'\2654\2655\2656\2657\2658\2659\265A\265B\265C\265D\265E\265F'
                    else E'\n'  
                  end
            )
            , '8', case when not $2 then '        ' else U&'\3000\3000\3000\3000\3000\3000\3000\3000' end) 
            , '7', case when not $2 then '       ' else U&'\3000\3000\3000\3000\3000\3000\3000' end) 
            , '6', case when not $2 then '      ' else U&'\3000\3000\3000\3000\3000\3000' end) 
            , '5', case when not $2 then '     ' else U&'\3000\3000\3000\3000\3000' end) 
            , '4', case when not $2 then '    ' else U&'\3000\3000\3000\3000' end) 
            , '3', case when not $2 then '   ' else U&'\3000\3000\3000' end) 
            , '2', case when not $2 then '  ' else U&'\3000\3000' end) 
            , '1', case when not $2 then ' ' else U&'\3000' end) 
        || E'\n' || split_part($1, ' ', 2)
        || '  '  || split_part($1, ' ', 3)
        || '  '  || split_part($1, ' ', 4)
        || '  '  || split_part($1, ' ', 5)
        || case when $3 then E'\n' || split_part($1::text, ' ', 1) else '' end
        || E'\n\n'
        
    ;
$$ LANGUAGE SQL IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION pretty(board, uni bool default false, fen bool default true)
RETURNS text AS $$
select pretty($1::text, $2, $3)
$$ LANGUAGE SQL IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION invert(board)
RETURNS text AS $$
    select translate($1::text, 'KQRBNPkqrbnpwb', 'kqrbnpKQRBNPbw')
$$ LANGUAGE SQL IMMUTABLE STRICT;
/*}}}*/
