MODULES = vihash
DATA_built = vihash.sql
DATA = uninstall_vihash.sql
REGRESS = vihash

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
