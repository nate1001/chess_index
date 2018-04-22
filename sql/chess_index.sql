
-- complain if script is sourced in psql, rather than via CREATE EXTENSION

--\echo Use "CREATE EXTENSION chess_index" to load this file. \quit


drop FUNCTION if exists square_in(cstring) cascade;
CREATE FUNCTION square_in(cstring)
RETURNS square
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

drop FUNCTION if exists square_out(square);
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

drop FUNCTION if exists piece_in(cstring) cascade;
CREATE FUNCTION piece_in(cstring)
RETURNS piece
AS '$libdir/chess_index'
LANGUAGE C IMMUTABLE STRICT;

drop FUNCTION if exists piece_out(piece);
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


