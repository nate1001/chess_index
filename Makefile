
EXTENSION = chess_index


REGRESS_OPTS  = --inputdir=test         \
                --load-extension=chess_index
REGRESS       = square

DATA = sql/chess_index--0.0.1.sql
#DATA         = $(filter-out $(wildcard sql/*--*.sql),$(wildcard sql/*.sql))
MODULES      = $(patsubst %.c,%,$(wildcard src/*.c))

# postgres build stuff
PG_CONFIG = pg_config

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

