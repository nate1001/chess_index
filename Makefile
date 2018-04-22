
EXTENSION = chess_index

#TESTS        = $(wildcard test/*.sql)
#REGRESS      = $(patsubst test/%.sql,%,$(TESTS))
REGRESS 	= test

DATA         = $(filter-out $(wildcard sql/*--*.sql),$(wildcard sql/*.sql))
MODULES      = $(patsubst %.c,%,$(wildcard src/*.c))

# postgres build stuff
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

