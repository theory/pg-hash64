CREATE OR REPLACE FUNCTION hash64(text) RETURNS bigint
AS 'hash64'
STRICT IMMUTABLE
LANGUAGE C;