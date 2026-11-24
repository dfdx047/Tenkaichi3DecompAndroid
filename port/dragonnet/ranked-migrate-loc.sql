-- Dragon Net Ranked: where a search comes from (port/netgeo/match.js: whole degrees, region, country, as Cloudflare
-- places the request), kept while it waits, so that near players are paired first. Additive; run once in the D1
-- console before deploying. Without it, searches pair as before (rating alone).
ALTER TABLE ranked_matches ADD COLUMN loc TEXT;
