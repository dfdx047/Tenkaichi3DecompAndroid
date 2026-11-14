ALTER TABLE ranked_matches ADD COLUMN series_id TEXT;
UPDATE ranked_matches SET series_id=id WHERE series_id IS NULL;
CREATE TABLE IF NOT EXISTS ranked_series_results (
 match_id TEXT PRIMARY KEY, series_id TEXT NOT NULL, host_wins INTEGER NOT NULL,
 guest_wins INTEGER NOT NULL, made INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS ranked_series_lookup ON ranked_series_results(series_id,made);
