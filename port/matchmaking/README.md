# Matchmaking Worker (room codes)

A small Cloudflare Worker that lets two players of the port find each other with a short code, without typing
addresses or forwarding a port. It only introduces the two games to each other: no game data passes through it.

Files: `worker.js` (the Worker), `schema.sql` (its one table, as ONE line without comments: the dashboard's console
joins what is pasted into one line, and a `--` comment then takes the rest of the statement with it).

The table `rooms`, one row per room (rows older than ten minutes are removed whenever a room is made):

| Column | |
|---|---|
| `code` | what the host tells the other player: 6 characters |
| `made` | seconds since 1970 |
| `version` | the game's netcode version: both players need the same |
| `ip` | the host's address as Cloudflare saw it (for the limit of rooms per address) |
| `host_key`, `join_key` | secrets handed to each player, so nobody else can read or end the room |
| `host_name`, `join_name` | the players' names |
| `host_addrs`, `join_addrs` | JSON: the addresses each game can be reached at ("ip:port"), best first |

| `queue` | a quick-match room: which queue it waits in (`S`, `T`, `D10`, `D15`, `D20`); empty for a room with a code to hand on |
| `who` | a quick-match room: a random text the host's game made up for this run, so that a room it left behind is not given back to it |

## Quick match (from 0.1.27)

A player who searches a queue is put with whoever waits longest in it, or, if nobody does, gets a room of their own
in that queue and waits there exactly as a host with a room code does (the same `poll`, `addrs`, `leave`; the same
relay rules and ceilings). Five queues: single battle, team battle, DP battle of 10, 15 and 20 DP; the game gives
all of them no time limit. A room lasts ten minutes; the game searches again by itself when it runs out. Limits:
60 searches an hour from one address, and the four open rooms per address that hosting has. One relay login is
asked for per search, not per room: a search that starts again says it holds a relay address already.

**Updating a Worker that is already set up** (in this order, so that nothing is refused in between):

1. D1 -> `bt3-rooms` -> Console. Run these two lines, one at a time:
   `ALTER TABLE rooms ADD COLUMN queue TEXT;`
   `ALTER TABLE rooms ADD COLUMN who TEXT;`
2. The Worker -> Edit code: replace everything with the new `worker.js`, Deploy.

Older releases of the game are not affected: they do not send the new request, and rooms with codes work as before.
Until the Worker is updated, a game that has the Quick match tab says "The matchmaking service does not know quick
match yet." when a search is started.

## Only the newest release (from 0.1.29)

The game tells the service which release it is (`rel`: 129 for 0.1.29). With the variable `MIN_RELEASE` set on the
Worker (Settings -> Variables and Secrets, a plain variable, e.g. `129`), a new room, a join or a quick-match search
from an older game is refused, and so is one from a game that does not say (0.1.28 and earlier). Rooms under way
are not touched. Without the variable, or at 0, everyone is let in.

- When its online screen opens, a game of 0.1.29 or later asks (`/v1/hello`) and, if it is below the minimum,
  shows at the top of that screen: "Update available. Online servers are unreachable until you update.", with its
  own number and the one that is needed. The same words answer a refused room, join or search.
- A second variable, `LATEST_RELEASE`, is optional: a game older than it, but not below the minimum, shows
  "Update available: release 0.1.NN." there and goes on working.
- Raise it when a release must be the minimum; it takes effect at the next request, with no new code.
- A game of 0.1.29 or later that is refused says so: "This release is too old ... Get the newest release."
  An older one shows its own words for an answer it does not know ("gave no room", "gave no answer").
- It closes the service, not the game: two copies of an old release can still play each other by typing an
  address (Host on a port / Join by address), and the source is open, so a changed copy can claim any number.

## The build key: only official releases (from the release after 0.1.33)

Anyone can build the game from this repository and point it at this Worker, which then spends its keeper's
allowance (the relay above all) and puts a changed game into quick match. So an official release can be built with
a **build key**, a secret of that one release, and signs every request with it; the Worker, once it has been told
the keys, can refuse what is not signed. All of it is off until the keys are set: a game built without a key sends
what it always sent, and a Worker without `BUILD_KEYS` answers as it always did.

**What it is not.** The key is inside every copy of the release (scrambled, so that it is not found by listing the
program's texts, and no better hidden than that): someone who takes the program apart has it. It keeps out a
build made from the source, not a determined person, and it is no check against cheating. That is why each
release has its own key and the Worker can forget one: a key that got out is worth something only until the
release it belongs to is retired.

**What a signed request carries**, as headers:

    X-BT3-Release: 134                  the release (0.1.34), as `rel` in the body
    X-BT3-Time:    1791900000           seconds since 1970
    X-BT3-Sign:    <64 hex digits>      HMAC-SHA256(key, text), in lower-case hex

and `text` is these six lines joined by a line feed (none after the last):

    BT3-SIGN-1
    POST
    /v1/quick                           the path
    134                                 the release, exactly as in the header
    1791900000                          the time, exactly as in the header
    <64 hex digits>                     SHA-256 of the body, exactly the bytes that are sent

The key is the text itself, as bytes (not decoded from hex). `port/src/gs/net_sign.c` is the game's side.

**The Worker's settings** (Settings -> Variables and Secrets):

| Name | Kind | |
|---|---|---|
| `BUILD_KEYS` | secret | the keys, as JSON: `{"134":"<key of 0.1.34>","135":"<key of 0.1.35>"}`. Not set: nothing is checked |
| `REQUIRE_KEY` | variable | what a failed check means: `off` (or not set), `quick`, `all` |
| `MIN_RELEASE` | variable | as before; a key of a release below it is not accepted either |

| `REQUIRE_KEY` | quick match (`/v1/quick`) | room codes, relay, queue counts | `/v1/hello` |
|---|---|---|---|
| no `BUILD_KEYS` at all | open | open | open |
| not set, or `off` | open; checked, logged, counted | open; checked, logged | open |
| `quick` | **signed only** | open; checked, logged | open |
| `all` | **signed only** | **signed only** | open |

A refused request is answered with status 403 and `{"error":"build key","why":"...","now":<the Worker's time>}`.
`why` is `none` (nothing was signed), `release` (the Worker has no key for that release; or it is below
`MIN_RELEASE`; or the header's release is not the body's), `sign` (not this key's signature of this request: a wrong
key, a changed body, another path), or `time` (signed for a time more than five minutes from the Worker's clock).
The game then says "Quick match needs an official release of the game." (in the online window, where a failed
search is reported), or for a room code "Room codes need an official release of the game. Playing by address
still works."

- **Clocks.** Players' clocks are often minutes or hours off. `/v1/hello`, which the game asks when its online
  screen opens, now also says the Worker's time, and the game signs by that; a request still refused for its time
  is sent once more by the `now` in the refusal. Only if that fails too does the player read "This computer's
  clock is wrong: set its date and time, then try again." Only a holder of the key is told `time`: the signature
  is checked first.
- **What it does not stop.** A signed request can be sent again unchanged for five minutes (the Worker keeps no
  record of signatures; the limits per address still count it). It cannot be changed: body, path, release and time
  are all in what is signed.
- **A `BUILD_KEYS` that cannot be read** (not JSON) gives the Worker no keys, so with `quick` or `all` it refuses
  everyone. Hence the first stage, `off`, and the check below.
- Nothing logs a key or a signature. A failed check is logged as `build key: quick from release 133: none,
  refused` (or `let in`), and counted per day.
- `REQUIRE_KEY` without `BUILD_KEYS` does nothing. Deleting `BUILD_KEYS` therefore opens the Worker again.

**Seeing what it does.** With `ADMIN_KEY` set (see the relay's ceiling below), the status request also answers

    "buildKey":{"releases":[134],"require":"off","today":{"ok":212,"none":40,"release":0,"sign":0,"time":3}}

the releases it has a key for (never a key), the stage, and how today's new rooms, joins and searches checked
out; `"problem"` is there when `BUILD_KEYS` or `REQUIRE_KEY` is not as it should be.

### Step by step, for the keeper

Nothing below needs a tool but a browser, and Python for one line. `<key>` stands for the key you make; it is
written nowhere in this repository and should not be pasted anywhere but the two places named.

**Once: the Worker's code.** Workers & Pages -> the Worker -> Edit code: replace everything with the new
`worker.js`, Deploy. (No change to the database.) Nothing changes for anyone yet.

**(a) Make a key.** On your own computer:

    python3 -c "import secrets; print(secrets.token_hex(32))"

It prints 64 letters and digits. Keep it in your password manager, under the release's number. Make a new one for
every release; never reuse one.

**(b) Give it to the build.** On GitHub: the repository -> Settings -> Secrets and variables -> Actions -> New
repository secret. Name `BT3_BUILD_KEY`, value `<key>`. (Later releases: the pencil beside the secret, paste the
new key.) GitHub never shows it again and hides it in logs. Only a tag's build (`v0.1.34`) is given it; the
build's log then has the line `build key: given`, else `build key: none`.

**(c) Tell the Worker.** Cloudflare: Workers & Pages -> the Worker -> Settings -> Variables and Secrets -> Add.
Type **Secret**, name `BUILD_KEYS`, value (one line, with the quotes, the number being the release: 0.1.34 is 134):

    {"134":"<key>"}

Deploy. Cloudflare does not show a secret again either: keep this whole line in your password manager, because
adding a release means pasting the whole line again with one more entry. Do this **before** the release is
published: a release whose key the Worker does not know is refused once (d) is on.

**(d) Switch it on, in stages.** The variable `REQUIRE_KEY` (type Text, not secret):

1. Leave it out (or `off`) at first. Publish the release. Nothing is refused; look at the status request's
   `buildKey` after a day: `releases` must list your release, there must be no `problem`, and `ok` should grow
   as players update. `sign` or `release` counts from the new release mean (b) and (c) do not hold the same key.
2. `quick`: quick match only for signed releases. Set `MIN_RELEASE` to the first release that has a key at the
   same time: older releases have no key, and unless they are told to update they show their own words for an
   answer they do not know ("The matchmaking service does not know quick match yet.").
3. `all`, if you want room codes and the relay closed to other builds as well.

Going back is the same variable: `off` refuses nothing, at once.

**(e) At each new release.** 1. Make a new key (a). 2. Replace the GitHub secret with it (b). 3. Add it to the
line in your password manager and paste the whole line as `BUILD_KEYS` again (c), e.g.
`{"134":"<key of 0.1.34>","135":"<key of 0.1.35>"}`. 4. Raise `PORT_RELEASE`, tag, let the workflow build, check
its log says `build key: given`. 5. Before publishing the draft, a check that costs nothing: the Linux program
started with `BT3_SIGN_TEST=1` prints `sign: build key yes ...` and ends (`build key none`: it has no key).

**Retiring a key** (a release was taken apart, or is simply old): raise `MIN_RELEASE` above that release (its
players are told to update), or paste `BUILD_KEYS` again without its entry (its players are told they need an
official release). The first is the kinder one; do both if the key is known to be out.

**A fork** that runs its own Worker makes its own keys, or none: without the secrets everything is as it was.

## What it keeps

For at most ten minutes per room: the room code, the two player names, the addresses each game reported about
itself, and the host's address as Cloudflare saw it (to limit how many rooms one address can open). Nothing else,
and nothing after the room has expired.

## Setting it up (the Cloudflare dashboard, no tools needed)

1. **Database.** Storage & Databases -> D1 -> Create. Name it `bt3-rooms`. Open its Console, paste the
   contents of `schema.sql`, run it.
2. **Worker.** Workers & Pages -> Create -> Worker ("Hello World"). Give it a name that says nothing about you
   (the name and your account's workers.dev subdomain are part of the public address). Deploy, then Edit code,
   replace everything with the contents of `worker.js`, Deploy.
3. **Binding.** The Worker -> Settings -> Bindings -> Add -> D1 database. Variable name `DB`, database
   `bt3-rooms`. Deploy.
4. Open the Worker's address in a browser: it answers with one line of text.

## The relay (optional)

Some pairs of players cannot reach each other directly (strict routers, mobile connections). With a TURN key the
host of a room also gets an address at Cloudflare's TURN service that passes the match's packets on.

1. Realtime -> TURN Server -> Create. Note the Turn Token ID and the API token (shown once).
2. The Worker -> Settings -> Variables and Secrets: add two **secrets**, `TURN_KEY_ID` and `TURN_KEY_TOKEN`. Deploy.

Without the two secrets the Worker works as before, without a relay. The key never leaves the Worker: each host is
handed a name and password that are good for three hours. Cloudflare's TURN is free for the first 1,000 GB a month
(a match is about 50 MB an hour, and only matches that cannot connect directly use it).

## A relay login only when a match needs one (from 0.1.29)

A game of 0.1.29 or later gets no relay login with its room. The two games first try to reach each other directly;
if they have not after four seconds, the host asks (`/v1/relay`, answered only once someone has joined), takes a
relay address, adds it to the room, and the one who joined learns it by asking the room again (`/v1/poll`). Logins
are then used by matches that need the relay, not by rooms: with a login for every room, the first day of quick
match used up the day's 500 on searches that never played through the relay, and the relay was off for everyone
until midnight UTC. An older game is still given its login with the room. `TURN_DAILY_MAX` (a plain variable)
raises the day's count if it is ever reached again; what protects the bill is the measuring below, not that count.

## A ceiling on the relay's use

Cloudflare has no spending limit for its TURN service (a budget alert only sends an e-mail, a day late), and a
relay login lets its holder send as much as they like for three hours. So the Worker measures the relay's use
itself, from Cloudflare's analytics, every five minutes:

- a login that has passed more than 1 GB in three hours is ended at once (a match is about 0.05 GB an hour;
  variable `TURN_LOGIN_GB`);
- when the calendar month's use reaches 500 GB (variable `TURN_MONTHLY_GB`; Cloudflare charges after 1,000),
  every login in use is ended and no more are given until the next month. Rooms go on working, without a relay.

Both directions are counted, which is more than is charged for. The analytics run some minutes behind, so the
ceiling can be passed by what flows in that time. To set it up:

1. Variable `CF_ACCOUNT_ID`: the account's ID (Workers & Pages overview, on the right).
2. Secret `CF_ANALYTICS_TOKEN`: an API token (My Profile -> API Tokens -> Create Token -> custom) with the single
   permission Account -> Account Analytics -> Read.
3. Secret `ADMIN_KEY`: 16 or more characters of your own choosing, for the status check below.
4. Settings -> Trigger Events -> Cron Trigger: `*/5 * * * *`.

Once `CF_ACCOUNT_ID` or `CF_ANALYTICS_TOKEN` is set, no relay login is given unless a measurement has succeeded in
the last two hours: a ceiling that cannot be checked is not relied on. To measure at once and see the result:

    curl -s -X POST $W/v1/status -d '{"admin":"<your ADMIN_KEY>"}'

answers for instance `{"measured":true,"monthGB":0.42,"limitGB":500,"relay":"on","loginsSeen":3,"loginsEnded":0,
"loginsToday":7,...}`, or `"measured":false` and why.

## What a player is told about the other

The name, the public address and port, and for the one who joins the host's relay address. An address of a
private network (192.168.x.x and the like) is handed over only when the two are on one network: seen by the
Worker under the same address (the same network part, for IPv6), or with the same public IPv4 address in what the
games found out themselves.

## Limits against abuse

Anyone can send requests to a Worker, so it counts (table `used`):

- an address may make 20 rooms an hour and have 4 open at a time;
- an address may enter 30 codes an hour that name no room (codes are not to be found by trying);
- an address is given at most 10 relay logins a day, and all addresses together 500 a day (the variable
  `TURN_DAILY_MAX` sets another number). Past that, rooms still work, without a relay. This is the ceiling on what
  the relay can be made to cost: a login lets its holder send through the relay for three hours.

Without the table `used` no relay logins are given at all (rooms work). What the counts do not prevent: many
addresses using up the Worker's own daily requests (100,000 on the free plan; matchmaking then stops until the
next day, at no cost). If a relay key is abused, deleting it (Realtime -> TURN Server) ends every login made from
it at once. A billing notification in the dashboard is worth setting.

## Testing a change

`node port/matchmaking/test.mjs` runs the Worker's code against a small stand-in for its database: who is told
which addresses, the limits, and the build key's checks in each stage. Nothing is sent anywhere.

`python3 port/matchmaking/test_sign.py port/build/bt3_64` checks the game's side of the build key against
Python's `hmac` and `hashlib`, and looks at the headers the program really sends, on a listener on 127.0.0.1 (see
the top of that file; with `BT3_BUILD_KEY` set to the made-up key the program was built with, or without one).

## Trying it by hand

    W=https://<worker>.<subdomain>.workers.dev
    curl -s -X POST $W/v1/host -d '{"v":3,"name":"A","addrs":["203.0.113.5:7000"]}'
    # -> {"code":"ABC234","key":"...","you":"<your address>"}
    curl -s -X POST $W/v1/join -d '{"v":3,"code":"ABC234","name":"B","addrs":["198.51.100.9:7000"]}'
    # -> {"key":"...","you":"...","peer":{"name":"A","addrs":["203.0.113.5:7000"]}}
    curl -s -X POST $W/v1/poll -d '{"code":"ABC234","key":"<the host key>"}'
    # -> {"peer":{"name":"B","addrs":["198.51.100.9:7000"]}}

## Limits

The free plan allows 100,000 requests a day. A host asks every two seconds while waiting, so ten minutes of
waiting are 300 requests. A room code is good for ten minutes. One address can have four rooms open.
