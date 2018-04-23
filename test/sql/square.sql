

create extension chess_index;

--fail
\echo 'fail'

select 'a0'::square;
select 'a9'::square;
select 'A8'::square;
select 'i1'::square;

select 'a1'::square = 'b1'::square;
select 'b1'::square < 'a2'::square;
select 'b1'::square <= 'a1'::square;
select 'a1'::square > 'b2'::square;
select 'a1'::square >= 'b2'::square;


\echo 'succeed'
select 'a1'::square;
select 'a1'::square = 'a1'::square;
select 'b1'::square > 'a2'::square;
select 'a1'::square >= 'a1'::square;
select 'b1'::square >= 'a2'::square;
select 'a1'::square < 'a2'::square;
select 'a1'::square <= 'a1'::square;
select 'a1'::square <= 'a2'::square;

\echo 'norows'
select a  from (select a,a = square_to_int(int_to_square(a)) as t from generate_series(0, 63) as a) as tt where t=false;

