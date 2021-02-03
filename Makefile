
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

CFLAGS += -I `pg_config --includedir`
LIBS = $(libpq_pgport)

all: copytester gencopyfuzz


clean:
	rm -f copytester.o gencopyfuzz.o copytester gencopyfuzz
