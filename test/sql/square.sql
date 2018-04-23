

create extension chess_index;

--fail
select 'a0'::square;
select 'a9'::square;
select 'A8'::square;
select 'i1'::square;


--succeed
select 'a1'::square;

-- no rows
select a  from (select a,a = square_to_int(int_to_square(a)) as t from generate_series(0, 63) as a) as tt where t=false;
